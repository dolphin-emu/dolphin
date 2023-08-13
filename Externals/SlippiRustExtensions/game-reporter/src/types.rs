/// The different modes that a player could be in.
///
/// Note that this type uses `serde_repr` to ensure we serialize the value (C-style)
/// and not the name itself.
#[derive(Copy, Clone, Debug, serde_repr::Serialize_repr, PartialEq, Eq)]
#[repr(u8)]
pub enum OnlinePlayMode {
    Ranked = 0,
    Unranked = 1,
    Direct = 2,
    Teams = 3,
}

/// Describes metadata about a game that we need to log to the server.
#[derive(Debug)]
pub struct GameReport {
    pub uid: String,
    pub play_key: String,
    pub online_mode: OnlinePlayMode,
    pub match_id: String,
    pub attempts: i32,
    pub duration_frames: u32,
    pub game_index: u32,
    pub tie_break_index: u32,
    pub winner_index: i8,
    pub game_end_method: u8,
    pub lras_initiator: i8,
    pub stage_id: i32,
    pub players: Vec<PlayerReport>,

    // This is set when we log the report. Anything before then
    // is a non-allocated `Vec<u8>` to just be a placeholder.
    pub replay_data: Vec<u8>,
}

/// Player metadata payload that's logged with game info.
#[derive(Debug, serde::Serialize)]
pub struct PlayerReport {
    #[serde(rename = "fbUid")]
    pub uid: String,

    #[serde(rename = "slotType")]
    pub slot_type: u8,

    #[serde(rename = "damageDone")]
    pub damage_done: f64,

    #[serde(rename = "stocksRemaining")]
    pub stocks_remaining: u8,

    #[serde(rename = "characterId")]
    pub character_id: u8,

    #[serde(rename = "colorId")]
    pub color_id: u8,

    #[serde(rename = "startingStocks")]
    pub starting_stocks: i64,

    #[serde(rename = "startingPercent")]
    pub starting_percent: i64,
}

/// The core report payload that's posted to the server.
#[derive(Debug, serde::Serialize)]
pub struct GameReportRequestPayload<'a> {
    #[serde(rename = "fbUid")]
    pub uid: &'a str,
    pub mode: OnlinePlayMode,
    pub players: &'a [PlayerReport],

    #[serde(rename = "isoHash")]
    pub iso_hash: &'a str,

    #[serde(rename = "matchId")]
    pub match_id: &'a str,

    #[serde(rename = "playKey")]
    pub play_key: &'a str,

    #[serde(rename = "gameDurationFrames")]
    pub duration_frames: u32,

    #[serde(rename = "gameIndex")]
    pub game_index: u32,

    #[serde(rename = "tiebreakIndex")]
    pub tie_break_index: u32,

    #[serde(rename = "winnerIdx")]
    pub winner_index: i8,

    #[serde(rename = "gameEndMethod")]
    pub game_end_method: u8,

    #[serde(rename = "lrasInitiator")]
    pub lras_initiator: i8,

    #[serde(rename = "stageId")]
    pub stage_id: i32,
}

impl<'a> GameReportRequestPayload<'a> {
    /// Builds a report request payload that can be serialized for POSTing
    /// to the server.
    pub fn with(report: &'a GameReport, iso_hash: &'a str) -> Self {
        Self {
            uid: &report.uid,
            play_key: &report.play_key,
            iso_hash,
            players: &report.players,
            match_id: &report.match_id,
            mode: report.online_mode,
            duration_frames: report.duration_frames,
            game_index: report.game_index,
            tie_break_index: report.tie_break_index,
            winner_index: report.winner_index,
            game_end_method: report.game_end_method,
            lras_initiator: report.lras_initiator,
            stage_id: report.stage_id,
        }
    }
}
