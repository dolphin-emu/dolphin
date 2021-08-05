#pragma once

#include <string>
#include <array>
#include <vector>
#include "Core/HW/Memmap.h"

//Const for structs
static const int cRosterSize = 9;
static const int cNumOfTeams = 2;

//Addrs for triggering evts
static const u32 aGameId           = 0x802EBF90;
static const u32 aEndOfGameFlag    = 0x80892AB3;
static const u32 aBattingResultSet = 0xFFFFFFFF;

//Addrs for GameInfo
static const u32 aTeam0_RosterCharId_Start = 0x80353C05;
static const u32 aTeam1_RosterCharId_Start = 0x803541A5;

static const u32 aTeam0_Captain = 0x80353083;
static const u32 aTeam1_Captain = 0x80353087;

static const u8 c_roster_table_offset = 0xa0;

//Addrs for DefensiveStats
static const u32 aPitcher_BattersFaced      = 0x803535C9;
static const u32 aPitcher_RunsAllowed       = 0x803535CA;
static const u32 aPitcher_BattersWalked     = 0x803535CE;
static const u32 aPitcher_BattersHit        = 0x803535D0;
static const u32 aPitcher_HitsAllowed       = 0x803535D2;
static const u32 aPitcher_HRsAllowed        = 0x803535D4;
static const u32 aPitcher_PitchesThrown     = 0x803535D6;
static const u32 aPitcher_Stamina           = 0x803535D8;
static const u32 aPitcher_WasPitcher        = 0x803535DA;
static const u32 aPitcher_BatterOuts        = 0x803535E1;
static const u32 aPitcher_StrikeOuts        = 0x803535E4;
static const u32 aPitcher_StarPitchesThrown = 0x803535E5;

static const u8 c_defensive_stat_offset = 0x1E;
//Addrs for OffensiveStats
static const u32 aBatter_AtBats       = 0x803537E8;
static const u32 aBatter_Hits         = 0x803537E9;
static const u32 aBatter_Singles      = 0x803537EA;
static const u32 aBatter_Doubles      = 0x803537EB;
static const u32 aBatter_Triples      = 0x803537EC;
static const u32 aBatter_Homeruns     = 0x803537ED;
static const u32 aBatter_Strikeouts   = 0x803537F1;
static const u32 aBatter_Walks_4Balls = 0x803537F2;
static const u32 aBatter_Walks_Hit    = 0x803537F3;
static const u32 aBatter_RBI          = 0x803537F4;
static const u32 aBatter_BasesStolen  = 0x803537F6;
static const u32 aBatter_BigPlays     = 0x80353807;
static const u32 aBatter_StarHits     = 0x80353808;

static const u8 c_offensive_stat_offset = 0x25;
//At-Bat Stats

class StatTracker{
public:
    //StatTracker() { };
    
    struct GameInfo{
        u32 game_id;
        std::string date_time;
        std::string p1_name;
        std::string p2_name;

        //Auto capture
        u16 team0_captain;
        u16 team1_captain;

        std::array<std::array<u8, cRosterSize>, cNumOfTeams> rosters_char_id;

        u8 team0_score;
        u8 team1_score;
    };
    GameInfo m_game_info;

    struct EndGameRosterDefensiveStats{
        u32 game_id;
        u32 team_id;
        u32 roster_id;

        u8  batters_faced;
        u16 runs_allowed;
        u16 batters_walked;
        u16 batters_hit;
        u16 hits_allowed;
        u16 homeruns_allowed;
        u16 pitches_thrown;
        u16 stamina;
        u8 was_pitcher;
        u8 batter_outs;
        u8 strike_outs;
        u8 star_pitches_thrown;

        u8 big_plays;

        u32 ERA;
    };
    std::array<std::array<EndGameRosterDefensiveStats, cRosterSize>, cNumOfTeams> m_defensive_stats;

    struct EndGameRosterOffensiveStats{
        u32 game_id;
        u32 team_id;
        u32 roster_id;

        u8 at_bats;
        u8 hits;
        u8 singles;
        u8 doubles;
        u8 triples;
        u8 homeruns;
        u8 strikouts;
        u8 walks_4balls;
        u8 walks_hit;
        u8 rbi;
        u8 bases_stolen;
        u8 star_hits;
    };
    std::array<std::array<EndGameRosterOffensiveStats, cRosterSize>, cNumOfTeams> m_offensive_stats;

    struct ABStats{
        u32 game_id;
        u32 team_id;
        u32 roster_id;

        //Scenario
        u32 inning;
        u32 outs;
        u32 strikes;
        u32 balls;
        u32 chem_links_ob;
        std::array<bool, 3> runner_ob; //1st and 2nd would be <true, true, false>
        u32 batter_stars;
        u32 is_star_chance;

        //Pitcher Status
        u32 pitcher_id;
        u32 pitch_type;
        u32 pitch_speed;
        u32 pitcher_stars;

        //Hit Status
        u32 hit_type;
        u32 left_right;
        u32 batter_handedness;
        u32 charge_power;
        
        //Final Result Ball
        u32 ball_angle;
        u32 ball_x_velocity;
        u32 ball_y_velocity;
        u32 ball_z_velocity;

        u32 result; //strike-swing, strike-looking, ball, foul, land (maybe bobble), caught
    };
    std::vector<ABStats> m_ab_stats;

    void init(){
        m_game_info = GameInfo();
        for (int team=0; team < cNumOfTeams; ++team){
            for (int roster=0; roster < cRosterSize; ++roster){
                m_defensive_stats[team][roster] = EndGameRosterDefensiveStats();
                m_offensive_stats[team][roster] = EndGameRosterOffensiveStats();
            }
        }
        m_ab_stats.clear();

        m_game_id = 0;
        m_end_of_game = std::make_pair(false, false);
        m_at_bat      = std::make_pair(false, false);

        m_stats_collected_for_game = false;
    }

    u32 m_game_id = 0;
    std::pair<bool, bool> m_end_of_game; //Prev, Current
    std::pair<bool, bool> m_at_bat; //Prev, Current

    bool m_stats_collected_for_game = false;

    void Run();
    void lookForTriggerEvents();
    void logGameInfo();
    void logDefensiveStats(int team_id, int roster_id);
    void logOffensiveStats(int team_id, int roster_id);
    void logAB();
    void printStatsToFile();
};
