
#include "Core/MSB_StatTracker.h"
#include <iostream>
#include <fstream>
#include <ctime>

void StatTracker::Run(){
    //GameId not set yet. Look for new game
    m_game_id = Memory::Read_U32(aGameId);
    if (m_game_id != 0 && !m_stats_collected_for_game){
        lookForTriggerEvents();
    }
    else if (m_game_id == 0) {
        init();
    }
}
void StatTracker::lookForTriggerEvents(){
    //End Of Game
    u8 end_of_game = Memory::Read_U8(aEndOfGameFlag);

    //Log Stats on the Rising Edge of EndOfGame
    m_end_of_game.second = m_end_of_game.first;
    m_end_of_game.first  = (end_of_game == 0x01);
    if (m_end_of_game.first && !m_end_of_game.second) {
        std::cout << "Game Has Ended" << std::endl;
        logGameInfo();

        printStatsToFile();
    }

    //At Bat
}

void StatTracker::logGameInfo(){
    time_t now = time(0);
    char* date_time = ctime(&now);
    m_game_info.date_time = std::string(date_time);

    m_game_info.team0_captain = Memory::Read_U8(aTeam0_Captain);
    m_game_info.team1_captain = Memory::Read_U8(aTeam1_Captain);

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
    stat.game_id   = m_game_id;
    stat.team_id   = team_id;
    stat.roster_id = roster_id;

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

void StatTracker::printStatsToFile(){
    std::string file_name = std::to_string(m_game_id) + ".txt";
    std::ofstream MyFile(file_name);

    MyFile << "{" << std::endl;
    MyFile << "  \"GameID\": \"" << m_game_id << "\"," << std::endl;
    MyFile << "  \"Date\": \"" << m_game_info.date_time << "\"," << std::endl;
    MyFile << "  \"Team1 Player\": \"<PLAYER1_NAME>\"," << std::endl;
    MyFile << "  \"Team2 Player\": \"<PLAYER2_NAME>\"," << std::endl;
    
    //Team Captain and Roster
    for (int team=0; team < cNumOfTeams; ++team){
        u16 captain_id = (team==0) ? m_game_info.team0_captain : m_game_info.team1_captain;
        MyFile << "  \"Team" << (team + 1) << " Captain\": \"" << std::to_string(captain_id) << "\"," << std::endl;
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
            MyFile << "      \"TeamId\": \"" << team << "\"," << std::endl;
            MyFile << "      \"RosterId\": \"" << roster << "\"," << std::endl;
            MyFile << "      \"Defensive Stats\": {" << std::endl;
            MyFile << "        \"Batters Faced\": \"" << std::to_string(def_stat.batters_faced) << "\"," << std::endl;
            MyFile << "        \"Runs Allowed\": \"" << def_stat.runs_allowed << "\"," << std::endl;
            MyFile << "        \"Batters Walked\": \"" << def_stat.batters_walked << "\"," << std::endl;
            MyFile << "        \"Batters Hit\": \"" << def_stat.batters_hit << "\"," << std::endl;
            MyFile << "        \"Hits Allowed\": \"" << def_stat.hits_allowed << "\"," << std::endl;
            MyFile << "        \"HRs Allowed\": \"" << def_stat.homeruns_allowed << "\"," << std::endl;
            MyFile << "        \"Pitches Thrown\": \"" << def_stat.pitches_thrown << "\"," << std::endl;
            MyFile << "        \"Stamina\": \"" << def_stat.stamina << "\"," << std::endl;
            MyFile << "        \"Was Pitcher\": \"" << std::to_string(def_stat.was_pitcher) << "\"," << std::endl;
            MyFile << "        \"Batter Outs\": \"" << std::to_string(def_stat.batter_outs) << "\"," << std::endl;
            MyFile << "        \"Strikeouts\": \"" << std::to_string(def_stat.strike_outs) << "\"," << std::endl;
            MyFile << "        \"Star Pitches Thrown\": \"" << std::to_string(def_stat.star_pitches_thrown) << "\"," << std::endl;
            MyFile << "        \"Big Plays\": \"" << std::to_string(def_stat.star_pitches_thrown) << "\"" << std::endl;
            MyFile << "      }," << std::endl;

            EndGameRosterOffensiveStats& of_stat = m_offensive_stats[team][roster];
            MyFile << "      \"Offensive Stats\": {" << std::endl;
            MyFile << "        \"AtBats\": \"" << std::to_string(of_stat.at_bats) << "\"," << std::endl;
            MyFile << "        \"Hits\": \"" << std::to_string(of_stat.hits) << "\"," << std::endl;
            MyFile << "        \"Singles\": \"" << std::to_string(of_stat.singles) << "\"," << std::endl;
            MyFile << "        \"Doubles\": \"" << std::to_string(of_stat.doubles) << "\"," << std::endl;
            MyFile << "        \"Triples\": \"" << std::to_string(of_stat.triples) << "\"," << std::endl;
            MyFile << "        \"Homeruns\": \"" << std::to_string(of_stat.homeruns) << "\"," << std::endl;
            MyFile << "        \"Strikeouts\": \"" << std::to_string(of_stat.strikouts) << "\"," << std::endl;
            MyFile << "        \"Walks (4 Balls)\": \"" << std::to_string(of_stat.walks_4balls) << "\"," << std::endl;
            MyFile << "        \"Walks (Hit)\": \"" << std::to_string(of_stat.walks_hit) << "\"," << std::endl;
            MyFile << "        \"RBI\": \"" << std::to_string(of_stat.rbi) << "\"," << std::endl;
            MyFile << "        \"Bases Stolen\": \"" << std::to_string(of_stat.bases_stolen) << "\"," << std::endl;
            MyFile << "        \"Star Hits\": \"" << std::to_string(of_stat.star_hits) << "\"" << std::endl;
            MyFile << "      }," << std::endl;
            MyFile << "    }" << std::endl;

            //Iterate Batters vector of batting stats
        }
    }
    MyFile << "  ]" << std::endl;
    MyFile << "}" << std::endl;
    m_stats_collected_for_game = true;
}