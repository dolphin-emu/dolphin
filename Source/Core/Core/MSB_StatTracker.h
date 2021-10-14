#pragma once

#include <string>
#include <array>
#include <vector>
#include "Core/HW/Memmap.h"

enum class GAME_STATE
{
  PREGAME,
  INGAME,
  ENDGAME_LOGGED,
};

enum class AB_STATE
{
    PITCH_STARTED,
    CONTACT,
    CONTACT_RESULT,
    LANDED,
    NO_CONTACT,
    PLAY_OVER,
    WAITING_FOR_PITCH,
};

//Const for structs
static const int cRosterSize = 9;
static const int cNumOfTeams = 2;

//Addrs for triggering evts
static const u32 aGameId           = 0x802EBF90;
static const u32 aEndOfGameFlag    = 0x80892AB3;

static const u32 aAB_PitchThrown     = 0x8088A81B;
static const u32 aAB_ContactResult   = 0x808926B3; //0=InAir, 1=Landed, 2=Fielded, 3=Caught, FF=Foul

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
static const u32 aPitcher_IsStarred = 0x8035323B;

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


//At-Bat Scenario 
static const u32 aAB_Team1IsBatting  = 0x80892997;
static const u32 aAB_RosterID        = 0x80890971;
static const u32 aAB_Inning          = 0x808928A3;
static const u32 aAB_HalfInning      = 0x8089294D;
static const u32 aAB_Balls           = 0x8089296F;
static const u32 aAB_Strikes         = 0x8089296B;
static const u32 aAB_Outs            = 0x80892973;
static const u32 aAB_P1_Stars        = 0x80892AD6;
static const u32 aAB_P2_Stars        = 0x80892AD7;
static const u32 aAB_IsStarChance    = 0x80892AD8;
static const u32 aAB_ChemLinksOnBase = 0x808909BA;

static const u32 aAB_RunnerOn1       = 0x8088F09D;
static const u32 aAB_RunnerOn2       = 0x8088F1F1;
static const u32 aAB_RunnerOn3       = 0x8088F345;

//At-Bat Pitch
static const u32 aAB_PitcherID         = 0x80890ADB;
static const u32 aAB_PitcherHandedness = 0x80890B01;
static const u32 aAB_PitchType         = 0x80890B21; //0=Curve, Charge=1, ChangeUp=2
static const u32 aAB_ChargePitchType   = 0x80890B1F; //2=Slider, 3=Perfect
static const u32 aAB_StarPitch         = 0x80890B34;
static const u32 aAB_PitchSpeed        = 0x80890B0A;

//At-Bat Hit
static const u32 aAB_BallVel_X      = 0x80890E50;
static const u32 aAB_BallVel_Y      = 0x80890E54;
static const u32 aAB_BallVel_Z      = 0x80890E58;
static const u32 aAB_BallAngle      = 0x808926D4;

static const u32 aAB_ChargeSwing    = 0x8089099B;
static const u32 aAB_Bunt           = 0x8089099B; //Bunt when =3 on contact
static const u32 aAB_ChargeUp       = 0x80890968;
static const u32 aAB_ChargeDown     = 0x8089096C;
static const u32 aAB_BatterHand     = 0x8089098B; //Right=0, Left=1
static const u32 aAB_InputDirection = 0x808909B9; //0=None, 1=PullingStickTowardsHitting, 2=PushStickAway
static const u32 aAB_StarSwing      = 0x808909B4;
static const u32 aAB_MoonShot       = 0x808909B5;
static const u32 aAB_TypeOfContact  = 0x808909A2; //0=Sour, 1=Nice, 2=Perfect, 3=Nice, 4=Sour

//At-Bat Miss
static const u32 aAB_Miss_SwingOrBunt = 0x808909A9; //(0=NoSwing, 1=Swing, 2=Bunt)
static const u32 aAB_Miss_AnyStrike = 0x80890B17;

//At-Bat Contact Result
static const u32 aAB_BallPos_X = 0x80890B38;
static const u32 aAB_BallPos_Y = 0x80890B3C;
static const u32 aAB_BallPos_Z = 0x80890B40;


static const u32 aAB_HitByPitch = 0x808909A3;
static const u32 aAB_BatterThrow_Tagged_Out = 0x80893B99; //0=Safe, 1=ThrownOutOrTagged, 255=


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
        u8 is_starred;

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

        //Calculated
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
        u8 roster_id;

        //Scenario
        u8 inning;
        u8 half_inning;
        u8 balls;
        u8 strikes;
        u8 outs;
        u8 runner_on_1;
        u8 runner_on_2;
        u8 runner_on_3;
        u8 chem_links_ob;
        u8 is_star_chance;
        u8 batter_stars;
        u8 pitcher_stars;

        //Pitcher Status
        u8 pitcher_id;
        u8 pitcher_handedness;
        u8 pitch_type;
        u8 charge_type;
        u8 star_pitch;
        u8 pitch_speed;

        //Hit Status
        u8 type_of_contact;
        u8 charge_swing;
        u8 bunt;
        u32 charge_power_up;
        u32 charge_power_down;
        u8 star_swing;
        u8 moon_shot;
        u8 input_direction;
        u8 batter_handedness;
        u8 hit_by_pitch;
    
        //  Ball Calcs
        u16 ball_angle;
        float ball_x_velocity;
        float ball_y_velocity;
        float ball_z_velocity;
        
        //Final Result Ball
        u32 ball_x_pos;
        u32 ball_y_pos;
        u32 ball_z_pos;

        u8 batter_out_at_1st; //Could be tag between HP and 1st or thrown out

        std::string result; //strike-swing, strike-looking, ball, foul, land, caught

        bool operator==(const ABStats& rhs_ab){
            return ( (this->inning      == rhs_ab.inning)
                  && (this->half_inning == rhs_ab.half_inning)
                  && (this->roster_id   == rhs_ab.roster_id)
                  && (this->balls       == rhs_ab.balls)
                  && (this->strikes     == rhs_ab.strikes)
                  && (this->outs        == rhs_ab.outs) );
        } 
    };
    std::array<std::array<std::vector<ABStats>, cRosterSize>, cNumOfTeams> m_ab_stats;

    void init(){
        m_game_info = GameInfo();
        for (int team=0; team < cNumOfTeams; ++team){
            for (int roster=0; roster < cRosterSize; ++roster){
                m_defensive_stats[team][roster] = EndGameRosterDefensiveStats();
                m_offensive_stats[team][roster] = EndGameRosterOffensiveStats();
                m_ab_stats[team][roster].clear();
            }
        }

        m_game_id = 0;

        //Reset state machines
        m_game_state = GAME_STATE::PREGAME;
        m_ab_state   = AB_STATE::WAITING_FOR_PITCH;
    }

    u32 m_game_id = 0;

    GAME_STATE m_game_state = GAME_STATE::PREGAME;
    AB_STATE   m_ab_state = AB_STATE::WAITING_FOR_PITCH;
    
    ABStats m_curr_ab_stat;

    bool mRankedStatus;
    bool mRecordStatus;
    void setRankedStatus(bool inBool);
    void setRecordStatus(bool inBool);

    void Run();
    void lookForTriggerEvents();

    void logGameInfo();
    void logDefensiveStats(int team_id, int roster_id);
    void logOffensiveStats(int team_id, int roster_id);
    
    void logABScenario();
    void logABMiss();
    void logABContact();
    void logABPitch();
    void logABContactResult();

    void printStatsToFile();
};
