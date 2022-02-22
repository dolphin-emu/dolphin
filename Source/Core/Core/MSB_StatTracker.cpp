
#include "Core/MSB_StatTracker.h"

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
        switch(m_event_state){
            case (EVENT_STATE::INIT):
                //Create new event, collect runner data
                m_game_info.getCurrentEvent() = Event();

                m_game_info.getCurrentEvent().runner_1 = logRunnerInfo(1);
                m_game_info.getCurrentEvent().runner_2 = logRunnerInfo(2);
                m_game_info.getCurrentEvent().runner_3 = logRunnerInfo(3);

                m_ab_state = AB_STATE::WAITING_FOR_PITCH;
                
            //Look for Pitch
            case (EVENT_STATE::WAITING_FOR_PITCH):
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

                //Init fielder tracker anytime fielder changes
                if (Memory::Read_U8(aAB_BatterPort)){
                    u8 team_id = (Memory::Read_U8(aAB_BatterPort) == m_game_info.away_port);
                    if ((Memory::Read_U8(aAB_BatterRosterID) != m_fielder_tracker[team_id].prev_batter_roster_loc)
                    || (m_fielder_tracker[team_id].anyUninitializedFielders())){
                        std::cout << "BatterSwitch TeamId=" << std::to_string(team_id) << std::endl;
                        m_fielder_tracker[team_id].prev_batter_roster_loc = Memory::Read_U8(aAB_BatterRosterID);
                        m_fielder_tracker[team_id].resetFielderMap();
                    }
                }

                //Trigger Events to look for
                //1. Are runners stealing and pitcher stepped off the mound
                //2. Has pitch started?
                //Watch for Runners Stealing
                if (Memory::Read_U8(aAB_PitchThrown)
                 || (areAnyRunnersStealing() && Memory::Read_U8(aAB_PickoffAttempt)){
                    //Event has started, log state
                    logEventState();

                    //Get batter runner info
                    m_game_info.getCurrentEvent().runner_0 = getRunnerInfo(0);

                    //Log if runners are stealing
                    logRunnerStealing(m_game_info.getCurrentEvent().runner_1);
                    logRunnerStealing(m_game_info.getCurrentEvent().runner_2);
                    logRunnerStealing(m_game_info.getCurrentEvent().runner_3);

                    if(Memory::Read_U8(aAB_PitchThrown)){
                        std::cout << "Pitch detected!" << std::endl;

                        //Check for fielder swaps
                        u8 team_id = (Memory::Read_U8(aAB_BatterPort) == m_game_info.away_port);
                        m_fielder_tracker[team_id].evaluateFielders();

                        //Check if pitcher was at center of mound, if so this is a potential DB
                        if (Memory::Read_U8(aFielder_Pos_X) == 0){
                            m_game_info.getCurrentEvent().pitch->potential_db = true;
                            std::cout << "Potential DB!" << std::endl;
                        }

                        //Pitch has started
                        m_ab_state = AB_STATE::PITCH_STARTED;
                    }
                    else if(Memory::Read_U8(aAB_PickoffAttempt)) {
                        m_ab_state = AB_STATE::MONITOR_RUNNERS;
                    }
                }


                //First Pitch of the game (once a game): collect port/player names
                if (Memory::Read_U8(aAB_PitchThrown) && (m_game_info.pitch_num == 0)){
                    //Collect port info for players
                    if (m_game_info.team0_port == 0xFF && m_game_info.team1_port == 0xFF){
                        u8 fielder_port = Memory::Read_U8(aAB_FieldingPort);
                        u8 batter_port = Memory::Read_U8(aAB_BattingPort);
                        
                        //Works off of the assumption that team0 is ALWAYS port1. Will not work in CPU v CPU games
                        if (fielder_port == 1) { m_game_info.team0_port = fielder_port; }
                        else if (batter_port == 1) { m_game_info.team0_port = batter_port; }
                        else { m_game_info.team0_port = 5; }

                        if (fielder_port > 1 && fielder_port <= 4){
                            m_game_info.team1_port = fielder_port;
                        }
                        if (batter_port > 1 && batter_port <= 4){
                            m_game_info.team1_port = batter_port;
                        }
                        else{
                            m_game_info.team1_port = 5;
                            //TODO - Disable stat submission here
                        }

                        //Map home and away ports for scores
                        m_game_info.away_port = (batter_port > 0 && batter_port <= 4) ? batter_port : 5;
                        m_game_info.home_port = (fielder_port > 0 && fielder_port <= 4) ? fielder_port : 5;

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

                    //Get captain roster positions
                    if ((m_game_info.team0_captain_roster_loc == 0xFF) || (m_game_info.team1_captain_roster_loc == 0xFF)) {
                        m_game_info.team0_captain_roster_loc = Memory::Read_U8(aTeam0_Captain_Roster_Loc);
                        m_game_info.team1_captain_roster_loc = Memory::Read_U8(aTeam1_Captain_Roster_Loc);
                    }
                }
                break;
            case (EVENT_STATE::PITCH_STARTED): //Look for contact or end of pitch
                //DBs
                //If the pitcher started in the center of the mound this is a potential DB
                //If the ball curves at any point it is no longer a DB
                if (m_game_info.getCurrentEvent().pitch->potential_db && (Memory::Read_U8(aAB_PitcherHasCtrlofPitch) == 1)) {
                    if (Memory::Read_U32(aAB_PitchCurveInput) != 0) {
                        std::cout << "No longer potential DB!" << std::endl;
                        m_game_info.getCurrentEvent().pitch->potential_db = false;
                    }
                }
                //HBP or miss
                if ((Memory::Read_U8(aAB_HitByPitch) == 1) || (Memory::Read_U8(aAB_PitchThrown) == 0)){
                    logEventPitch(m_game_info.getCurrentEvent().pitch.get());
                    logContactMiss(m_game_info.getCurrentEvent().pitch->contact.get()); //Strike or Swing or Bunt
                    m_event_state = EVENT_STATE::PLAY_OVER;
                }
                //Contact
                else if (Memory::Read_U32(aAB_BallVel_X))){
                    //Init contact
                    m_game_info.getCurrentEvent().pitch->contact = Contact();
                    
                    logEventPitch(m_game_info.getCurrentEvent().pitch);
                    logContact(m_game_info.getCurrentEvent().pitch.contact.get());
                    m_event_state = EVENT_STATE::CONTACT;
                }
                //Else just keep looking for contact or end of pitch
                break;
            case (EVENT_STATE::CONTACT):
                if (Memory::Read_U8(aAB_ContactResult)){
                    //Indicate that pitch resulted in contact and log contact details
                    m_game_info.getCurrentEvent().pitch->pitch_result = 6;
                    logContactResult(m_game_info.getCurrentEvent().pitch->contact.get()); //Land vs Caught vs Foul, Landing POS.
                    if(m_event_state != EVENT_STATE::LOG_FIELDER) { //If we don't need to scan for which fielder fields the ball
                        m_event_state = EVENT_STATE::PLAY_OVER;
                    }
                }
                else {
                    //Ball is still in air
                    Contact* contact = m_game_info.getCurrentEvent().pitch->contact;
                    contact->prev_ball_x_pos = contact->ball_x_pos;
                    contact->prev_ball_y_pos = contact->ball_y_pos;
                    contact->prev_ball_z_pos = contact->ball_z_pos;
                    contact->ball_x_pos = Memory::Read_U32(aAB_BallPos_X);
                    contact->ball_y_pos = Memory::Read_U32(aAB_BallPos_Y);
                    contact->ball_z_pos = Memory::Read_U32(aAB_BallPos_Z);
                }
                //Could bobble before the ball hits the ground.
                //Search for bobble if we haven't recorded one yet and the ball hasn't been collected yet
                if (!m_game_info.getCurrentEvent().pitch->contact->fielder_bobble.has_value() 
                 && !m_game_info.getCurrentEvent().pitch->contact->fielder_final.has_value()){
                     
                    //Returns a fielder that has bobbled if any exist. Otherwise optional is nullptr
                    m_game_info.getCurrentEvent().pitch->contact->fielder_bobble = logFielderBobble();
                }
                break;
            case (EVENT_STATE::LOG_FIELDER):
                {
                //Look for bobble if we haven't seen any fielder touch the ball yet
                if (!m_game_info.getCurrentEvent().pitch->contact->fielder_bobble.has_value() 
                 && !m_game_info.getCurrentEvent().pitch->contact->fielder_final.has_value()){
                    auto& fielder_bobble = m_game_info.getCurrentEvent().pitch->contact->fielder_bobble;
                    //Returns a fielder that has bobbled if any exist. Otherwise optional is nullptr
                    fielder_bobble = logFielderBobble();
                }
                else if (!fielder_final.has_value()){
                    auto& fielder_final = m_game_info.getCurrentEvent().pitch->contact->fielder_final;
                    //Returns fielder that is holding the ball. Otherwise nullptr
                    fielder_final = logFielderWithBall();
                    if (fielder_final.has_value()){
                        //Start watching runners for outs when the ball has finally been collected
                        m_event_state = EVENT_STATE::MONITOR_RUNNERS;
                    }
                }

                //Break out if play ends without fielding the ball (HR or other play ending hit)
                if (!Memory::Read_U8(aAB_PitchThrown)) {
                    m_event_state = EVENT_STATE::PLAY_OVER;
                }
                break;
            case (EVENT_STATE::MONITOR_RUNNERS):
                if (!Memory::Read_U8(aAB_PitchThrown) && !Memory::Read_U8(aAB_PickoffAttempt)){
                    m_event_state = EVENT_STATE::PLAY_OVER;
                }
                else {
                    logRunnerEvents(m_game_info.getCurrentEvent().runner_batter);
                    logRunnerEvents(m_game_info.getCurrentEvent().runner_1);
                    logRunnerEvents(m_game_info.getCurrentEvent().runner_1);
                    logRunnerEvents(m_game_info.getCurrentEvent().runner_1);

                }
                break;
            case (EVENT_STATE::PLAY_OVER):
                if (!Memory::Read_U8(aAB_PitchThrown)){
                    event.rbi = Memory::Read_U8(aAB_RBI);
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

                m_game_info.getCurrentEvent().result_game = Memory::Read_U8(aAB_FinalResult);

                //runner_batter out, contact_secondary
                logFinalResults(m_game_info.getCurrentEvent());

                //Increment event count
                ++m_game_info.event_num;
                
                m_event_state = EVENT_STATE::INIT;
                std::cout << "Logging Final Result" << std::endl << "Waiting for next pitch..." << std::endl;
                std::cout << std::endl;
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
            if ((Memory::Read_U8(aEndOfGameFlag) == 1) && (m_event_state == EVENT_STATE::WAITING_FOR_PITCH) ){
                logGameInfo();

                std::pair<std::string, std::string> jsonPlusPath = getStatJSON(true);
                File::WriteStringToFile(jsonPlusPath.second, jsonPlusPath.first);
                std::cout << "Logging to " << jsonPlusPath.second << std::endl;

                std::pair<std::string, std::string> jsonPlusPathEnoded = getStatJSON(false);
                File::WriteStringToFile(jsonPlusPathEnoded.second + ".encoded", jsonPlusPathEnoded.first);

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

    std::time_t unix_time = std::time(nullptr);

    m_game_info.unix_date_time = std::to_string(unix_time);
    m_game_info.local_date_time = std::asctime(std::localtime(&unix_time));
    m_game_info.local_date_time.pop_back();

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
    u32 offset = (team_id * cRosterSize * c_defensive_stat_offset) + (roster_id * c_defensive_stat_offset);

    u32 is_starred_offset = (team_id * cRosterSize) + roster_id;

    //in_team_id is in terms of team0 and team1 but we need it to 
    u8 adjusted_team_id;
    //Map team 0 and 1 to home and away
    if (in_team_id == 0){
        adjusted_team_id = (m_game_info.team0_port == m_game_info.away_port);
    }
    else{
        adjusted_team_id = (m_game_info.team1_port == m_game_info.away_port);
    }
    auto& stat = character_summaries[adjusted_team_id][roster_id].end_game_defensive_stats;

    character_summaries[adjusted_team_id][roster_id].is_starred = Memory::Read_U8(aPitcher_IsStarred + is_starred_offset);

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
    auto& stat = character_summaries[adjusted_team_id][roster_id].end_game_offensive_stats;

    stat.at_bats        = Memory::Read_U8(aBatter_AtBats + offset);
    stat.hits           = Memory::Read_U8(aBatter_Hits + offset);
    stat.singles        = Memory::Read_U8(aBatter_Singles + offset);
    stat.doubles        = Memory::Read_U8(aBatter_Doubles + offset);
    stat.triples        = Memory::Read_U8(aBatter_Triples + offset);
    stat.homeruns       = Memory::Read_U8(aBatter_Homeruns + offset);
    stat.bunt_successes = Memory::Read_U8(aBatter_BuntSuccess + offset);
    stat.sac_flys       = Memory::Read_U8(aBatter_SacFlys + offset);
    stat.strikouts      = Memory::Read_U8(aBatter_Strikeouts + offset);
    stat.walks_4balls   = Memory::Read_U8(aBatter_Walks_4Balls + offset);
    stat.walks_hit      = Memory::Read_U8(aBatter_Walks_Hit + offset);
    stat.rbi            = Memory::Read_U8(aBatter_RBI + offset);
    stat.bases_stolen   = Memory::Read_U8(aBatter_BasesStolen + offset);
    stat.star_hits      = Memory::Read_U8(aBatter_StarHits + offset);

    character_summaries[adjusted_team_id][roster_id].end_game_defensive_stats.big_plays = Memory::Read_U8(aBatter_BigPlays + offset);
}

void StatTracker::logEventState(Event& in_event){
    std::cout << "Logging Event State" << std::endl;

    //Team Id (0=Home is batting, Away is batting)
    in_event.batting_team_id = (Memory::Read_U8(aAB_BatterPort) == m_game_info.away_port);
    in_event.roster_id = Memory::Read_U8(aAB_RosterID);

    in_event.inning          = Memory::Read_U8(aAB_Inning);
    in_event.half_inning     = !in_event.batting_team_id;

    //Figure out scores
    in_event.away_score = Memory::Read_U16(aAwayTeam_Score);
    in_event.home_score = Memory::Read_U16(aHomeTeam_Score);

    in_event.balls           = Memory::Read_U8(aAB_Balls);
    in_event.strikes         = Memory::Read_U8(aAB_Strikes);
    in_event.outs            = Memory::Read_U8(aAB_Outs);
    
    //Figure out star ownership
    if (in_game_info.team0_port == m_game_info.away_port){
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
}

void StatTracker::logContact(Contact* in_contact){
    std::cout << "Logging Contact" << std::endl;

    in_contact->type_of_contact   = Memory::Read_U8(aAB_TypeOfContact);
    in_contact->bunt              =(Memory::Read_U8(aAB_Bunt) == 3);

    in_contact->swing             = (Memory::Read_U8(aAB_Miss_SwingOrBunt) == 1);
    in_contact->charge_swing      = ((Memory::Read_U8(aAB_ChargeSwing) == 1) && !(Memory::Read_U8(aAB_StarSwing)));
    in_contact->charge_power_up   = Memory::Read_U32(aAB_ChargeUp);
    in_contact->charge_power_down = Memory::Read_U32(aAB_ChargeDown);
    in_contact->star_swing        = Memory::Read_U8(aAB_StarSwing);
    in_contact->moon_shot         = Memory::Read_U8(aAB_MoonShot);
    in_contact->input_direction   = Memory::Read_U8(aAB_InputDirection);

    in_contact->horiz_power       = Memory::Read_U16(aAB_HorizPower);
    in_contact->vert_power        = Memory::Read_U16(aAB_VertPower);
    in_contact->ball_angle        = Memory::Read_U16(aAB_BallAngle);

    in_contact->ball_x_velocity   = Memory::Read_U32(aAB_BallVel_X);
    in_contact->ball_y_velocity   = Memory::Read_U32(aAB_BallVel_Y);
    in_contact->ball_z_velocity   = Memory::Read_U32(aAB_BallVel_Z);

    //in_contact->ball_x_accel   = Memory::Read_U32(aAB_BallAccel_X);
    //in_contact->ball_y_accel   = Memory::Read_U32(aAB_BallAccel_Y);
    //in_contact->ball_z_accel   = Memory::Read_U32(aAB_BallAccel_Z);

    in_contact->hit_by_pitch = Memory::Read_U8(aAB_HitByPitch);

    in_contact->ball_x_pos_upon_hit = Memory::Read_U32(aAB_BallPos_X_Upon_Hit);
    in_contact->ball_y_pos_upon_hit = Memory::Read_U32(aAB_BallPos_Z_Upon_Hit);

    in_contact->ball_x_pos_upon_hit = Memory::Read_U32(aAB_BatterPos_X_Upon_Hit);
    in_contact->ball_y_pos_upon_hit = Memory::Read_U32(aAB_BatterPos_Z_Upon_Hit);

    //Frame collect
    in_contact->frameOfSwingUponContact = Memory::Read_U16(aAB_FrameOfSwingAnimUponContact);
}

void StatTracker::F(Event& in_event){
    std::cout << "Logging Miss" << std::endl;

    Pitch* pitch = event.pitch.get();
    Contact* contact = event.pitch->contact.get();

    contact->type_of_contact   = 0xFF; //Set 0 because game only sets when contact is made. Never reset
    contact->bunt              =(Memory::Read_U8(aAB_Miss_SwingOrBunt) == 2); //Need to use miss version for bunt. Game won't set bunt regular flag unless contact is made
    contact->charge_swing      = ((Memory::Read_U8(aAB_ChargeSwing) == 1) && !(Memory::Read_U8(aAB_StarSwing)));
    contact->charge_power_up   = Memory::Read_U32(aAB_ChargeUp);
    contact->charge_power_down = Memory::Read_U32(aAB_ChargeDown);

    contact->star_swing        = Memory::Read_U8(aAB_StarSwing);
    contact->moon_shot         = Memory::Read_U8(aAB_MoonShot);
    contact->input_direction   = Memory::Read_U8(aAB_InputDirection);

    contact->horiz_power       = 0;
    contact->vert_power        = 0;
    contact->ball_angle        = 0;

    contact->ball_x_velocity   = 0;
    contact->ball_y_velocity   = 0;
    contact->ball_z_velocity   = 0;

    contact->ball_x_pos_upon_hit = Memory::Read_U32(aAB_BallPos_X_Upon_Hit);
    contact->ball_y_pos_upon_hit = Memory::Read_U32(aAB_BallPos_Z_Upon_Hit);

    contact->ball_x_pos_upon_hit = Memory::Read_U32(aAB_BatterPos_X_Upon_Hit);
    contact->ball_y_pos_upon_hit = Memory::Read_U32(aAB_BatterPos_Z_Upon_Hit);

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
        else if (event.balls == 3) { pitch->pitch_result = 1; }
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

void StatTracker::logEventPitch(Pitch* in_pitch){
    std::cout << "Logging Pitching" << std::endl;

    //Add this inning to the list of innings that the pitcher has pitched in
    in_pitch->pitcher_team_id    = Memory::Read_U8(aAB_PitcherPort) == m_game_info.away_port;
    in_pitch->pitcher_roster_loc = Memory::Read_U8(aAB_PitcherRosterID);
    in_pitch->pitcher_char_id    = Memory::Read_U8(aAB_PitcherID);
    in_pitch->pitch_type         = Memory::Read_U8(aAB_PitchType);
    in_pitch->charge_type        = Memory::Read_U8(aAB_ChargePitchType);
    in_pitch->star_pitch         = Memory::Read_U8(aAB_StarPitch);
    in_pitch->pitch_speed        = Memory::Read_U8(aAB_PitchSpeed);
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
    else if (result == 3){
        in_contact->primary_contact_result = 0; //Out (secondary=caught)
        in_contact->secondary_contact_result = 0; //Out (secondary=caught)

        //If the ball is caught, use the balls position from the frame before to avoid the ball_pos
        //from matching the fielders
        contact->ball_x_pos = contact->prev_ball_x_pos;
        contact->ball_y_pos = contact->prev_ball_y_pos;
        contact->ball_z_pos = contact->prev_ball_z_pos; 

        //Ball has been caught. Log this as final fielder. If ball has been bobbled they will be logged as bobble
        std::optional<Fielder> final_fielder = logFielderWithBall();
        in_contact->fielder_final = final_fielder;

        //Increment outs for that position for fielder
        ++m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].out_count_by_position[in_contact->fielder_final.fielder_roster_loc][in_contact->fielder_final.fielder_pos];
        //Indicate if fielder had been swapped for this batter
        std::cout << "Trying to see if fielder was swapped. Team_id=" << (!m_game_info.getCurrentEvent().half_inning) 
                  << " Fielder Roster=" << std::to_string(m_curr_ab_stat.fielder_roster_loc)
                  << " Swapped=" << m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].fielder_map[in_contact->fielder_final.fielder_roster_loc].second << std::endl;
        in_contact->fielder_final.fielder_swapped_for_batter = m_fielder_tracker[!m_game_info.getCurrentEvent().half_inning].fielder_map[in_contact->fielder_final.fielder_roster_loc].second;
    }
    else if (result == 0xFF){
        in_contact->primary_contact_result = 1; //Foul
        in_contact->secondary_contact_result = 3; //Foul
        in_contact->ball_x_pos = Memory::Read_U32(aAB_BallPos_X);
        in_contact->ball_y_pos = Memory::Read_U32(aAB_BallPos_Y);
        in_contact->ball_z_pos = Memory::Read_U32(aAB_BallPos_Z);
    }
    else{
        in_contact->primary_contact_result = 3;
        in_contact->ball_x_pos = Memory::Read_U32(aAB_BallPos_X);
        in_contact->ball_y_pos = Memory::Read_U32(aAB_BallPos_Y);
        in_contact->ball_z_pos = Memory::Read_U32(aAB_BallPos_Z);
    }
}

void StatTracker::logFinalResults(Event& in_event){
    std::cout << "Logging Final Result" << std::endl;

    //Indicate strikeout in the runner_0
    if (in_event.game_result == 1){
        //0x10 in runner_0 denotes strikeout
        in_event.runner_0.out_type = 0x10;
    }

    //Fill in secondary result for contact
    if (in_event.game_result >= 0x7 && in_event.game_result <= 0xF){
        //0x10 in runner_0 denotes strikeout
        in_event.pitch.value()->contact.value()->secondary_contact_result = in_event.game_result;
    }
    else if ((in_event.runner_0.out_type == 2) || (in_event.runner_0.out_type == 3)){
        in_event.pitch.value()->contact.value()->secondary_contact_result = in_event.runner_0.out_type;
    }

    //multi_out
    in_event.pitch.value()->contact.value()->multi_out = Memory::Read_U8(aAB_NumOutsDuringPlay) > 1;
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
    std::string date_time = (inDecode) ? m_game_info.local_date_time : m_game_info.unix_date_time;
    json_stream << "  \"GameID\": \"" << std::hex << m_game_info.game_id << "\"," << std::endl;
    json_stream << "  \"Date\": \"" << date_time << "\"," << std::endl;
    json_stream << "  \"Ranked\": " << std::to_string(m_game_info.ranked) << "," << std::endl;
    json_stream << "  \"StadiumID\": " << stadium << "," << std::endl;
    json_stream << "  \"Away Player\": \"" << away_player_name << "\"," << std::endl; //TODO MAKE THIS AN ID
    json_stream << "  \"Home Player\": \"" << home_player_name << "\"," << std::endl;

    json_stream << "  \"Away Score\": " << std::dec << m_game_info.away_score << "," << std::endl;
    json_stream << "  \"Home Score\": " << std::dec << m_game_info.home_score << "," << std::endl;

    json_stream << "  \"Innings Selected\": " << std::to_string(m_game_info.innings_selected) << "," << std::endl;
    json_stream << "  \"Innings Played\": " << std::to_string(m_game_info.innings_played) << "," << std::endl;
    
    //Team Captain and Roster
    for (int team=0; team < cNumOfTeams; ++team){
        //If Team0 is away team
        std::string team_label;
        if (team == 0){
            if (team0_is_away){
                team_label = "Away";
            }
            else {
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
    }
    json_stream << "  \"Quitter Team\": \"" << m_game_info.quitter_team << "\"," << std::endl;

    json_stream << "  \"Character Game Stats\": [" << std::endl;
    //Defensive Stats
    for (int team=0; team < cNumOfTeams; ++team){
        // std::string team_label;
        // u8 captain_roster_loc = 0;
        // if (team == 0){
        //    captain_roster_loc = m_game_info.team0_captain_roster_loc;
        //    if (team0_is_away){
        //        team_label = "Away";
        //    }
        //    else{
        //        team_label = "Home";
        //    }
        // }
        // else if (team == 1) {
        //    captain_roster_loc = m_game_info.team1_captain_roster_loc;
        //    if (team0_is_away){
        //        team_label = "Home";
        //    }
        //    else {
        //        team_label = "Away";
        //    }
        // }

        for (int roster=0; roster < cRosterSize; ++roster){
            json_stream << "    {" << std::endl;
            json_stream << "      \"Team\": \"" << team << "\"," << std::endl;
            json_stream << "      \"RosterID\": " << roster << "," << std::endl;
        }

        for (int roster=0; roster < cRosterSize; ++roster){
            EndGameRosterDefensiveStats& def_stat = m_defensive_stats[team][roster];
            json_stream << "    {" << std::endl;
            json_stream << "      \"Team\": \"" << team_label << "\"," << std::endl;
            json_stream << "      \"RosterID\": " << roster << "," << std::endl;

            std::string character = (inDecode) ? "\"" + cCharIdToCharName.at(m_game_info.rosters_char_id[team][roster]) + "\"" : std::to_string(m_game_info.rosters_char_id[team][roster]);
            json_stream << "      \"Character\": " << character << "," << std::endl;
            json_stream << "      \"Captain\": " << (captain_roster_loc == roster) << "," << std::endl;
            json_stream << "      \"Superstar\": " << std::to_string(def_stat.is_starred) << "," << std::endl;
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
            json_stream << "        \"Strikeouts\": " << std::to_string(def_stat.strike_outs) << "," << std::endl;
            json_stream << "        \"Star Pitches Thrown\": " << std::to_string(def_stat.star_pitches_thrown) << "," << std::endl;
            json_stream << "        \"Big Plays\": " << std::to_string(def_stat.star_pitches_thrown) << "," << std::endl;
            json_stream << "        \"Outs Pitched\": " << std::to_string(def_stat.outs_pitched) << "," << std::endl;
            json_stream << "        \"Inning Appearances\": " << std::to_string(def_stat.innings_pitched.size()) << "," << std::endl;
            json_stream << "        \"Pitches Per Position\": [" << std::endl;
            if (m_fielder_tracker[team].pitchesAtAnyPosition(roster, 0)){
                json_stream << "          {" << std::endl;
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].pitch_count_by_position[roster][pos] > 0){
                        std::string comma = (m_fielder_tracker[team].pitchesAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].pitch_count_by_position[roster][pos]) << comma << std::endl;
                    }
                }
                json_stream << "          }" << std::endl;
            }
            json_stream << "        ]," << std::endl;
            json_stream << "        \"Outs Per Position\": [" << std::endl;
            if (m_fielder_tracker[team].outsAtAnyPosition(roster, 0)){
                json_stream << "          {" << std::endl;
                for (int pos = 0; pos < cNumOfPositions; ++pos) {
                    if (m_fielder_tracker[team].out_count_by_position[roster][pos] > 0){
                        std::string comma = (m_fielder_tracker[team].outsAtAnyPosition(roster, pos+1)) ? "," : "";
                        json_stream << "            \"" << cPosition.at(pos) << "\": " << std::to_string(m_fielder_tracker[team].out_count_by_position[roster][pos]) << comma << std::endl;
                    }
                }
                json_stream << "          }" << std::endl;
            }
            json_stream << "        ]" << std::endl;
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
            json_stream << "      \"Pitch Summary\": [" << std::endl;
            for (auto it = m_ab_stats[team][roster].begin(); it != m_ab_stats[team][roster].end(); it++){
                ABStats ab_stat = *it;

                json_stream << "        {" << std::endl;
                json_stream << "          \"Pitch Num\": " << ab_stat.pitch_num << "," << std::endl;
                json_stream << "          \"Batter\": " << character << "," << std::endl;
                json_stream << "          \"Inning\": " << std::to_string(ab_stat.inning) << "," << std::endl;
                json_stream << "          \"Half Inning\": " << std::to_string(ab_stat.half_inning) << "," << std::endl;
                json_stream << "          \"Batter Score\": " << std::dec << ab_stat.batter_score << "," << std::endl;
                json_stream << "          \"Fielder Score\": " << std::dec << ab_stat.fielder_score << "," << std::endl;
                json_stream << "          \"Balls\": " << std::to_string(ab_stat.balls) << "," << std::endl;
                json_stream << "          \"Strikes\": " << std::to_string(ab_stat.strikes) << "," << std::endl;
                json_stream << "          \"Outs\": " << std::to_string(ab_stat.outs) << "," << std::endl;
                json_stream << "          \"Runners on Base\": [" << std::to_string(ab_stat.runner_on_3) << "," 
                                                             << std::to_string(ab_stat.runner_on_2) << ","
                                                             << std::to_string(ab_stat.runner_on_1) << "]," << std::endl;
                json_stream << "          \"Chemistry Links on Base\": " << std::to_string(ab_stat.chem_links_ob) << "," << std::endl;
                json_stream << "          \"Star Chance\": " << std::to_string(ab_stat.is_star_chance) << "," << std::endl;
                json_stream << "          \"Batter Stars\": " << std::to_string(ab_stat.away_stars) << "," << std::endl;
                json_stream << "          \"Pitcher Stars\": " << std::to_string(ab_stat.home_stars) << "," << std::endl;

                std::string pitcher       = (inDecode) ? "\"" + cCharIdToCharName.at(ab_stat.pitcher_char_id) + "\"" : std::to_string(ab_stat.pitcher_char_id);
                std::string pitcher_hand  = (inDecode) ? "\"" + cHandToHR.at(ab_stat.pitcher_handedness) + "\"" : std::to_string(ab_stat.pitcher_handedness);
                std::string pitch_type    = (inDecode) ? "\"" + cPitchTypeToHR.at(ab_stat.pitch_type) + "\"" : std::to_string(ab_stat.pitch_type);
                std::string charge_pitch_type    = (inDecode) ? "\"" + cChargePitchTypeToHR.at(ab_stat.charge_type) + "\"" : std::to_string(ab_stat.charge_type);
                json_stream << "          \"Pitcher Roster Location\": " << std::to_string(ab_stat.pitcher_roster_loc) << "," << std::endl;
                json_stream << "          \"PitcherID\": " << pitcher << "," << std::endl;
                json_stream << "          \"Pitcher Stamina\": " << std::dec << ab_stat.pitcher_stamina << "," << std::endl;
                json_stream << "          \"Pitcher Handedness\": " << pitcher_hand << "," << std::endl;
                json_stream << "          \"Pitch Type\": " << pitch_type << "," << std::endl;
                json_stream << "          \"Charge Pitch Type\": " << charge_pitch_type << "," << std::endl;
                json_stream << "          \"Star Pitch\": " << std::to_string(ab_stat.star_pitch) << "," << std::endl;
                json_stream << "          \"Pitch Speed\": " << std::to_string(ab_stat.pitch_speed) << "," << std::endl;
                json_stream << "          \"Pitch DB (Integrosity)\": " << std::to_string(ab_stat.db) << "," << std::endl;

                
                std::string batter_handedness  = (inDecode) ? "\"" + cHandToHR.at(ab_stat.batter_handedness) + "\"" : std::to_string(ab_stat.batter_handedness);
                json_stream << "          \"Batter Handedness\": " << batter_handedness << "," << std::endl;

                //Create TypeOfSwing
                u8 typeOfSwing;
                if (ab_stat.star_swing == 1){ typeOfSwing = 3; }
                else if (ab_stat.charge_swing == 1){ typeOfSwing = 2; }
                else if (ab_stat.bunt == 1){ typeOfSwing = 4; }
                else if ((ab_stat.swing == 1) 
                      || (ab_stat.type_of_contact != 0xFF) ) { typeOfSwing = 1; }
                else { typeOfSwing = 0; }
                ab_stat.type_of_swing = typeOfSwing;
                std::string type_of_swing = (inDecode) ? "\"" + cTypeOfSwing.at(ab_stat.type_of_swing) + "\"" : std::to_string(ab_stat.type_of_swing);

                json_stream << "          \"Type of Swing\": " << type_of_swing << "," << std::endl;
                json_stream << "          \"Contact Summary\": [" << std::endl;
                
                if (ab_stat.type_of_contact != 0xFF) {
                    json_stream << "            {" << std::endl;
                    std::string type_of_contact    = (inDecode) ? "\"" + cTypeOfContactToHR.at(ab_stat.type_of_contact) + "\"" : std::to_string(ab_stat.type_of_contact);
                    std::string input_direction    = (inDecode) ? "\"" + cInputDirectionToHR.at(ab_stat.input_direction) + "\"" : std::to_string(ab_stat.input_direction);
                    json_stream << "              \"Type of Contact\": " << type_of_contact << "," << std::endl;

                    //Convert Charge u32s to IEEE 754 Floats
                    float charge_power_up, charge_power_down;
                    float_converter.num = ab_stat.charge_power_up;
                    charge_power_up = float_converter.fnum;

                    float_converter.num = ab_stat.charge_power_down;
                    charge_power_down = float_converter.fnum;

                    json_stream << "              \"Charge Power Up\": " << charge_power_up << "," << std::endl;
                    json_stream << "              \"Charge Power Down\": " << charge_power_down << "," << std::endl;
                    json_stream << "              \"Star Swing Five-Star\": " << std::to_string(ab_stat.moon_shot) << "," << std::endl;
                    json_stream << "              \"Input Direction\": " << input_direction << "," << std::endl;
                    json_stream << "              \"Frame Of Swing Upon Contact\": " << std::dec << ab_stat.frameOfSwingUponContact << "," << std::endl;
                    json_stream << "              \"Ball Angle\": \"" << std::dec << ab_stat.ball_angle << "\"," << std::endl;
                    json_stream << "              \"Ball Vertical Power\": \"" << std::dec << ab_stat.vert_power << "\"," << std::endl;
                    json_stream << "              \"Ball Horizontal Power\": \"" << std::dec << ab_stat.horiz_power << "\"," << std::endl;


                    //Convert velocity, pos u32s to IEEE 754 Floats
                    float ball_x_velocity, ball_y_velocity, ball_z_velocity,
                        ball_x_pos, ball_y_pos, ball_z_pos,
                        ball_x_pos_upon_hit, ball_y_pos_upon_hit;
                    float_converter.num = ab_stat.ball_x_velocity;
                    ball_x_velocity = float_converter.fnum;

                    float_converter.num = ab_stat.ball_y_velocity;
                    ball_y_velocity = float_converter.fnum;

                    float_converter.num = ab_stat.ball_z_velocity;
                    ball_z_velocity = float_converter.fnum;

                    float_converter.num = ab_stat.ball_x_pos;
                    ball_x_pos = float_converter.fnum;

                    float_converter.num = ab_stat.ball_y_pos;
                    ball_y_pos = float_converter.fnum;

                    float_converter.num = ab_stat.ball_z_pos;
                    ball_z_pos = float_converter.fnum;

                    float_converter.num = ab_stat.ball_x_pos_upon_hit;
                    ball_x_pos_upon_hit = float_converter.fnum;

                    float_converter.num = ab_stat.ball_y_pos_upon_hit;
                    ball_y_pos_upon_hit = float_converter.fnum;

                    json_stream << "              \"Ball Velocity - X\": " << floatConverter() << "," << std::endl;
                    json_stream << "              \"Ball Velocity - Y\": " << floatConverter() << "," << std::endl;
                    json_stream << "              \"Ball Velocity - Z\": " << floatConverter() << "," << std::endl;
                    //json_stream << "              \"Ball Acceleration - X\": " << ball_x_accel << "," << std::endl;
                    //json_stream << "              \"Ball Acceleration - Y\": " << ball_y_accel << "," << std::endl;
                    //json_stream << "              \"Ball Acceleration - Z\": " << ball_z_accel << "," << std::endl;
                    json_stream << "              \"Ball Landing Position - X\": " << floatConverter() << "," << std::endl;
                    json_stream << "              \"Ball Landing Position - Y\": " << floatConverter() << "," << std::endl;
                    json_stream << "              \"Ball Landing Position - Z\": " << floatConverter() << "," << std::endl;

                    json_stream << "              \"Ball Position Upon Contact - X\": " << floatConverter() << "," << std::endl;
                    json_stream << "              \"Ball Position Upon Contact - Y\": " << floatConverter() << "," << std::endl;

                    json_stream << "              \"Fielding Summary\": [" << std::endl;
                    if (ab_stat.fielder_roster_loc != 0xFF) {
                        json_stream << "                 {" << std::endl;
                        std::string fielder_pos  = (inDecode) ? "\"" + cPosition.at(ab_stat.fielder_pos) + "\"" : std::to_string(ab_stat.fielder_pos);
                        std::string fielder_char = (inDecode) ? "\"" + cCharIdToCharName.at(ab_stat.fielder_char_id) + "\"" : std::to_string(ab_stat.fielder_char_id);
                        
                        //std::string fielder_action = (inDecode) ? "\"" + cFielderActions.at(ab_stat.fielder_action) + "\"" : std::to_string(ab_stat.fielder_action);
                        //std::string fielder_bobble = (inDecode) ? "\"" + cFielderBobbles.at(ab_stat.bobble) + "\"" : std::to_string(ab_stat.bobble);

                        json_stream << "                   \"Fielder Roster Location\": " << std::to_string(ab_stat.fielder_roster_loc) << "," << std::endl;
                        json_stream << "                   \"Fielder Position\": " << fielder_pos << "," << std::endl;
                        json_stream << "                   \"Fielder Character\": " << fielder_char << "," << std::endl;
                        json_stream << "                   \"Fielder Action\": " << std::to_string(ab_stat.fielder_action) << "," << std::endl;
                        json_stream << "                   \"Fielder Swap\": " <<  std::to_string(m_curr_ab_stat.fielder_swapped_for_batter) << "," << std::endl;
                        
                        float fielder_x_pos, fielder_y_pos, fielder_z_pos;

                        float_converter.num = ab_stat.fielder_x_pos;
                        fielder_x_pos = float_converter.fnum;

                        float_converter.num = ab_stat.fielder_y_pos;
                        fielder_y_pos = float_converter.fnum;

                        float_converter.num = ab_stat.fielder_z_pos;
                        fielder_z_pos = float_converter.fnum;

                        json_stream << "                   \"Fielder Position - X\": " << fielder_x_pos << "," << std::endl;
                        json_stream << "                   \"Fielder Position - Y\": " << fielder_y_pos << "," << std::endl;
                        json_stream << "                   \"Fielder Position - Z\": " << fielder_z_pos << "," << std::endl;
                        json_stream << "                   \"Fielder Bobble\": " << std::to_string(ab_stat.bobble) << "," << std::endl;
                        json_stream << "                   \"Bobble Summary\": [" <<  std::endl;
                        if (ab_stat.bobble != 0xFF && (ab_stat.fielder_pos != ab_stat.bobble_fielder_pos)) {
                            json_stream << "                   {" << std::endl;
                            std::string bobble_fielder_pos  = (inDecode) ? "\"" + cPosition.at(ab_stat.bobble_fielder_pos) + "\"" : std::to_string(ab_stat.bobble_fielder_pos);
                            std::string bobble_fielder_char = (inDecode) ? "\"" + cCharIdToCharName.at(ab_stat.bobble_fielder_char_id) + "\"" : std::to_string(ab_stat.bobble_fielder_char_id);
                            //std::string bobble_fielder_action = (inDecode) ? "\"" + cFielderActions.at(ab_stat.bobble_fielder_action) + "\"" : std::to_string(ab_stat.bobble_fielder_action);

                            json_stream << "                     \"Bobble Fielder Roster Location\": " << std::to_string(ab_stat.bobble_fielder_roster_loc) << "," << std::endl;
                            json_stream << "                     \"Bobble Fielder Position\": " << bobble_fielder_pos << "," << std::endl;
                            json_stream << "                     \"Bobble Fielder Character\": " << bobble_fielder_char << "," << std::endl;
                            json_stream << "                     \"Fielder Action\": " << std::to_string(ab_stat.bobble_fielder_action) << "," << std::endl;
                            
                            float bobble_fielder_x_pos, bobble_fielder_y_pos, bobble_fielder_z_pos;

                            float_converter.num = ab_stat.bobble_fielder_x_pos;
                            bobble_fielder_x_pos = float_converter.fnum;

                            float_converter.num = ab_stat.bobble_fielder_y_pos;
                            bobble_fielder_y_pos = float_converter.fnum;

                            float_converter.num = ab_stat.bobble_fielder_z_pos;
                            bobble_fielder_z_pos = float_converter.fnum;

                            json_stream << "                     \"Bobble Fielder Position - X\": " << bobble_fielder_x_pos << "," << std::endl;
                            json_stream << "                     \"Bobble Fielder Position - Y\": " << bobble_fielder_y_pos << "," << std::endl;
                            json_stream << "                     \"Bobble Fielder Position - Z\": " << bobble_fielder_z_pos << std::endl;
                            json_stream << "                   }" << std::endl;
                        }
                        json_stream << "                   ]" <<  std::endl;
                        json_stream << "                 }" << std::endl;
                    }
                    
                    json_stream << "              ]" << std::endl;
                    json_stream << "            }" << std::endl;
                }

                json_stream << "          ]," << std::endl;
                json_stream << "          \"Number Outs During Play\": " << std::to_string(ab_stat.num_outs_during_play) << "," << std::endl;
                json_stream << "          \"RBI\": " << std::to_string(ab_stat.rbi) << "," << std::endl;

                json_stream << "          \"Final Result - Inferred\": \"" << ab_stat.result_inferred << "\"," << std::endl;
                json_stream << "          \"Final Result - Game\": " << std::to_string(ab_stat.result_game) << std::endl;
                
                std::string end_comma = (std::next(it) != m_ab_stats[team][roster].end()) ? "," : "";
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

//Scans player for possession
std::optional<Fielder> StatTracker::logFielderWithBall(Fielder fielder) {
    std::optional<Fielder> fielder;
    for (u8 pos=0; pos < cRosterSize; ++pos){
        u32 aFielderControlStatus = aFielder_ControlStatus + (pos * cFielder_Offset);
        u32 aFielderPosX = aFielder_Pos_X + (pos * cFielder_Offset);
        u32 aFielderPosY = aFielder_Pos_Y + (pos * cFielder_Offset);
        u32 aFielderPosZ = aFielder_Pos_Z + (pos * cFielder_Offset);

        u32 aFielderJump = aFielder_AnyJump + (pos * cFielder_Offset);
        u32 aFielderAction = aFielder_Action + (pos * cFielder_Offset);

        bool fielder_has_ball = (Memory::Read_U8(aFielderControlStatus) == 0xA);

        if (fielder_has_ball) {
            Fielder fielder_with_ball;
            //get char id
            fielder_with_ball.roster_id = Memory::Read_U8(aFielderControlStatus-0x5A);
            fielder_with_ball.char_id = Memory::Read_U8(aFielderControlStatus-0x58); //0x58 is the diff between the fielder control status and the char id
            fielder_with_ball.fielder_pos = pos;

            fielder_with_ball.x_pos = Memory::Read_U32(aFielderPosX);
            fielder_with_ball.y_pos = Memory::Read_U32(aFielderPosY);
            fielder_with_ball.z_pos = Memory::Read_U32(aFielderPosZ);

            u8 action = 0x0;

            if (Memory::Read_U32(aFielderAction)) {
                fielder_with_ball.action = Memory::Read_U8(aFielderAction); //2 = Slide, 3 = Walljump

                std::cout << "Type of Action: " << std::to_string(action) << std::endl;
                if (Memory::Read_U8(aFielderAction) == 1) {
                    fielder_with_ball.action = 0xFF;
                }
            }
            else if (Memory::Read_U8(aFielderJump)) {
                fielder_with_ball.action = Memory::Read_U8(aFielderJump); //1 = jump
                if (Memory::Read_U8(aFielderAction) != 1) {
                    fielder_with_ball.action = 0xFE;
                }
            }

            std::cout << "Logging Fielder" << std::endl;
            fielder = std::make_optional(fielder_with_ball);
            return fielder;
        }
    }
    return fielder;
}

std::optional<Fielder> StatTracker::logFielderBobble() {
    std::optional<Fielder> fielder;
    for (u8 pos=0; pos < cRosterSize; ++pos){
        u32 aFielderControlStatus = aFielder_ControlStatus + (pos * cFielder_Offset);
        u32 aFielderBobbleStatus = aFielder_Bobble + (pos * cFielder_Offset);
        u32 aFielderKnockoutStatus = aFielder_Knockout + (pos * cFielder_Offset);

        u32 aFielderJump = aFielder_AnyJump + (pos * cFielder_Offset);
        u32 aFielderAction = aFielder_Action + (pos * cFielder_Offset);

        u32 aFielderPosX = aFielder_Pos_X + (pos * cFielder_Offset);
        u32 aFielderPosY = aFielder_Pos_Y + (pos * cFielder_Offset);
        u32 aFielderPosZ = aFielder_Pos_Z + (pos * cFielder_Offset);
        
        u8 typeOfFielderDisruption = 0x0;
        u8 bobble_addr = Memory::Read_U8(aFielderBobbleStatus);
        u8 knockout_addr = Memory::Read_U8(aFielderKnockoutStatus);

        if (knockout_addr) {
            typeOfFielderDisruption = 0x10; //Knockout - no bobble
        }
        else if (bobble_addr){
            typeOfFielderDisruption = bobble_addr; //Different types of bobbles
        }

        if (typeOfFielderDisruption > 0x0) {
            Fielder fielder_that_bobbled;
            std::cout << "Type of Bobble: " << std::to_string(typeOfFielderDisruption) << std::endl;
            //get char id
            fielder_that_bobbled.roster_id = Memory::Read_U8(aFielderControlStatus-0x5A);
            fielder_that_bobbled.char_id = Memory::Read_U8(aFielderControlStatus-0x58); //0x58 is the diff between the fielder control status and the char id

            fielder_that_bobbled.x_pos = Memory::Read_U32(aFielderPosX);
            fielder_that_bobbled.y_pos = Memory::Read_U32(aFielderPosY);
            fielder_that_bobbled.z_pos = Memory::Read_U32(aFielderPosZ);

            u8 action = 0x0;

            if (Memory::Read_U8(aFielderAction)) {
                fielder_that_bobbled.action = Memory::Read_U8(aFielderAction); //2 = Slide, 3 = Walljump
                std::cout << "Type of Action: " << std::to_string(action) << std::endl;
                if (Memory::Read_U8(aFielderAction) == 1) {
                    fielder_that_bobbled.action = 0xFF;
                }
            }
            else if (Memory::Read_U8(aFielderJump)) {
                fielder_that_bobbled.action = Memory::Read_U8(aFielderJump); //1 = jump
                if (Memory::Read_U8(aFielderAction) != 1) {
                    fielder_that_bobbled.action = 0xFE;
                }
            }

            fielder_that_bobbled.bobble = typeOfFielderDisruption;

            std::cout << "Logging Bobble" << std::endl;
            fielder = std::make_optional(fielder_with_ball);
            return fielder;
        }
    }
    return fielder;
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
        if (m_game_info.team0_port == 5) m_game_info.team0_player_name = "CPU";
        if (m_game_info.team1_port == 5) m_game_info.team1_player_name = "CPU";
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

std::optional<Runner> StatTracker::logRunnerInfo(u8 base){
    optional<Runner> runner;
    //See if there is a runner in this pos
    if (Memory::Read_U8(aRunner_RosterLoc + (base * cRunner_Offset)) != 0xFF){
        Runner init_runner;
        init_runner.roster_loc = Memory::Read_U8(aRunner_RosterLoc + (base * cRunner_Offset));
        init_runner.char_id = Memory::Read_U8(aRunner_CharId + (base * cRunner_Offset));
        init_runner.initial_base = base;
        runner = std::make_unique(init_runner);
        return runner;        
    }
    return runner;
}

void StatTracker::logRunnerStealing(std::optional<Runner> in_runner){
    //Return if no runner
    if (!in_runner.has_value()) { return; }

    //Set stealing val
    //0-No stealing
    //1-Ready to steal
    //2-regular steal
    //3-perfect steal
    u8 steal_val = Memory::Read_U8(aRunner_Stealing + (in_runner.initial_base * cRunner_Offset));
}

void StatTracker::logRunnerEvents(std::optional<Runner> in_runner in_runner){
    //Return if no runner
    if (!in_runner.has_value()) { return; }

    //Return if runner has already gotten out
    if (in_runner->out_type != 0) { return; }
    else {
        in_runner->out_type = Memory::Read_U8(aRunner_OutType + (in_runner->initial_base * cRunner_Offset));
        if (in_runner->out_type != 0) {
            in_runner->out_location = Memory::Read_U8(aRunner_CurrentBase + (in_runner->initial_base * cRunner_Offset));
            in_runner->result_base = 0xFF;
        }
        else{
            in_runner->result_base = Memory::Read_U8(aRunner_CurrentBase + (in_runner->initial_base * cRunner_Offset));
        }
    }
}