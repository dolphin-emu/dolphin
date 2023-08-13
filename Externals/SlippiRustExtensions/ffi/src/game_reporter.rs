use std::ffi::c_char;

use slippi_game_reporter::{GameReport, OnlinePlayMode as ReporterOnlinePlayMode, PlayerReport};

use crate::{c_str_to_string, set};

/// This enum is duplicated from `slippi_game_reporter::OnlinePlayMode` in order
/// to appease cbindgen, which cannot see the type from the other module for
/// inspection.
///
/// This enum will likely go away as things move towards Rust, since it's effectively
/// just C FFI glue code.
#[derive(Debug)]
#[repr(C)]
pub enum SlippiMatchmakingOnlinePlayMode {
    Ranked = 0,
    Unranked = 1,
    Direct = 2,
    Teams = 3,
}

/// Creates a new Player Report and leaks it, returning the pointer.
///
/// This should be passed on to a GameReport for processing.
#[no_mangle]
pub extern "C" fn slprs_player_report_create(
    uid: *const c_char,
    slot_type: u8,
    damage_done: f64,
    stocks_remaining: u8,
    character_id: u8,
    color_id: u8,
    starting_stocks: i64,
    starting_percent: i64,
) -> usize {
    let uid = c_str_to_string(uid, "slprs_player_report_create", "uid");

    let report = Box::new(PlayerReport {
        uid,
        slot_type,
        damage_done,
        stocks_remaining,
        character_id,
        color_id,
        starting_stocks,
        starting_percent,
    });

    let report_instance_ptr = Box::into_raw(report) as usize;

    report_instance_ptr
}

/// Creates a new GameReport and leaks it, returning the instance pointer
/// after doing so.
///
/// This is expected to ultimately be passed to the game reporter, which will handle
/// destruction and cleanup.
#[no_mangle]
pub extern "C" fn slprs_game_report_create(
    uid: *const c_char,
    play_key: *const c_char,
    online_mode: SlippiMatchmakingOnlinePlayMode,
    match_id: *const c_char,
    duration_frames: u32,
    game_index: u32,
    tie_break_index: u32,
    winner_index: i8,
    game_end_method: u8,
    lras_initiator: i8,
    stage_id: i32,
) -> usize {
    let fn_name = "slprs_game_report_create";
    let uid = c_str_to_string(uid, fn_name, "user_id");
    let play_key = c_str_to_string(play_key, fn_name, "play_key");
    let match_id = c_str_to_string(match_id, fn_name, "match_id");

    let report = Box::new(GameReport {
        uid,
        play_key,

        online_mode: match online_mode {
            SlippiMatchmakingOnlinePlayMode::Ranked => ReporterOnlinePlayMode::Ranked,
            SlippiMatchmakingOnlinePlayMode::Unranked => ReporterOnlinePlayMode::Unranked,
            SlippiMatchmakingOnlinePlayMode::Direct => ReporterOnlinePlayMode::Direct,
            SlippiMatchmakingOnlinePlayMode::Teams => ReporterOnlinePlayMode::Teams,
        },

        match_id,
        attempts: 0,
        duration_frames,
        game_index,
        tie_break_index,
        winner_index,
        game_end_method,
        lras_initiator,
        stage_id,
        players: Vec::new(),
        replay_data: Vec::new(),
    });

    let report_instance_ptr = Box::into_raw(report) as usize;

    report_instance_ptr
}

/// Takes ownership of the `PlayerReport` at the specified pointer, adding it to the
/// `GameReport` at the corresponding pointer.
#[no_mangle]
pub extern "C" fn slprs_game_report_add_player_report(instance_ptr: usize, player_report_instance_ptr: usize) {
    // Coerce the instance from the pointer. This is theoretically safe since we control
    // the C++ side and can guarantee that the `game_report_instance_ptr` is only owned
    // by us, and is created/destroyed with the corresponding lifetimes.
    let player_report = unsafe { Box::from_raw(player_report_instance_ptr as *mut PlayerReport) };

    set::<GameReport, _>(instance_ptr, move |report| {
        report.players.push(*player_report);
    });
}
