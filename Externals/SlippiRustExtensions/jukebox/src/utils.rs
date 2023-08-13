use crate::scenes::scene_ids::*;
use crate::tracks::{get_track_id_by_filename, TrackId};
use crate::Result;
use anyhow::anyhow;
use std::collections::HashMap;
use std::ffi::CStr;
use std::io::{Read, Seek};

/// Get an unsigned 24 bit integer from a byte slice
fn read_u24(bytes: &[u8], offset: usize) -> u32 {
    let size = 3;
    let end = offset + size;
    let mut padded_bytes = [0; 4];
    padded_bytes[1..4].copy_from_slice(&bytes[offset..end]);
    u32::from_be_bytes(padded_bytes)
}

/// Get an unsigned 32 bit integer from a byte slice
fn read_u32(bytes: &[u8], offset: usize) -> u32 {
    let size = (u32::BITS / 8) as usize;
    let end: usize = offset + size;
    u32::from_be_bytes(
        bytes[offset..end]
            .try_into()
            .unwrap_or_else(|_| unreachable!("u32::BITS / 8 is always 4")),
    )
}

/// Get a copy of the `size` bytes in `file` at `offset`
pub(crate) fn read_from_file(file: &mut std::fs::File, offset: u64, size: usize) -> Result<Vec<u8>> {
    file.seek(std::io::SeekFrom::Start(offset))?;
    let mut bytes = vec![0; size];
    file.read_exact(&mut bytes)?;
    Ok(bytes)
}

/// Produces a hashmap containing offsets and sizes of .hps files contained within the iso
/// These can be looked up by TrackId
pub(crate) fn create_track_map(iso: &mut std::fs::File) -> Result<HashMap<TrackId, (u32, u32)>> {
    const FST_LOCATION_OFFSET: u64 = 0x424;
    const FST_SIZE_OFFSET: u64 = 0x0428;
    const FST_ENTRY_SIZE: usize = 0xC;

    // Filesystem Table (FST)
    let fst_location = u32::from_be_bytes(
        read_from_file(iso, FST_LOCATION_OFFSET, 0x4)?
            .try_into()
            .map_err(|_| anyhow!("Unable to read FST offset as u32"))?,
    );
    if fst_location <= 0 {
        return Err(anyhow!("FST location is 0"));
    }

    let fst_size = u32::from_be_bytes(
        read_from_file(iso, FST_SIZE_OFFSET, 0x4)?
            .try_into()
            .map_err(|_| anyhow!("Unable to read FST size as u32"))?,
    );
    // Check for sensible size. Vanilla is 29993
    if fst_size < 1000 || fst_size > 500000 {
        return Err(anyhow!("FST size is out of range"));
    }

    let fst = read_from_file(iso, fst_location as u64, fst_size as usize)?;

    // FST String Table.
    // TODO: This is the line that crashed on CISO. All above values are 0.
    // I think a better fix than the defensive lines I added above could be to
    // return Result<...> from read_u32 and handle errors? With the code as is,
    // it's still technically possible maybe to get through here and then crash
    // on the read_u32 calls in the filter_map
    let str_table_offset = read_u32(&fst, 0x8) as usize * FST_ENTRY_SIZE;

    // Collect the .hps file metadata in the FST into a hash map
    Ok(fst[..str_table_offset]
        .chunks(FST_ENTRY_SIZE)
        .filter_map(|entry| {
            let is_file = entry[0] == 0;
            let name_offset = str_table_offset + read_u24(entry, 0x1) as usize;
            let offset = read_u32(entry, 0x4);
            let size = read_u32(entry, 0x8);

            let name = CStr::from_bytes_until_nul(&fst[name_offset..]).ok()?.to_str().ok()?;

            if is_file && name.ends_with(".hps") {
                match get_track_id_by_filename(&name) {
                    Some(track_id) => Some((track_id, (offset, size))),
                    None => None,
                }
            } else {
                None
            }
        })
        .collect())
}

/// Returns a tuple containing a randomly selected menu track tournament track
/// to play
pub(crate) fn get_random_menu_tracks() -> (TrackId, TrackId) {
    // 25% chance to use the alternate menu theme
    let menu_track = if fastrand::u8(0..4) == 0 {
        TrackId::Menu2
    } else {
        TrackId::Menu1
    };

    // 50% chance to use the alternate tournament mode theme
    let tournament_track = if fastrand::u8(0..2) == 0 {
        TrackId::TournamentMode1
    } else {
        TrackId::TournamentMode2
    };

    (menu_track, tournament_track)
}

/// Returns true if the user is in an actual match
/// Sourced from M'Overlay: https://github.com/bkacjios/m-overlay/blob/d8c629d/source/melee.lua#L1177
pub(crate) fn is_in_game(scene_major: u8, scene_minor: u8) -> bool {
    if scene_major == SCENE_ALL_STAR_MODE && scene_minor < SCENE_ALL_STAR_CSS {
        return true;
    }
    if scene_major == SCENE_VS_MODE || scene_major == SCENE_VS_ONLINE {
        return scene_minor == SCENE_VS_INGAME;
    }
    if (SCENE_TRAINING_MODE..=SCENE_STAMINA_MODE).contains(&scene_major) || scene_major == SCENE_FIXED_CAMERA_MODE {
        return scene_minor == SCENE_TRAINING_INGAME;
    }
    if scene_major == SCENE_EVENT_MATCH {
        return scene_minor == SCENE_EVENT_MATCH_INGAME;
    }
    if scene_major == SCENE_CLASSIC_MODE && scene_minor < SCENE_CLASSIC_CONTINUE {
        return scene_minor % 2 == 1;
    }
    if scene_major == SCENE_ADVENTURE_MODE {
        return scene_minor == SCENE_ADVENTURE_MUSHROOM_KINGDOM
            || scene_minor == SCENE_ADVENTURE_MUSHROOM_KINGDOM_BATTLE
            || scene_minor == SCENE_ADVENTURE_MUSHROOM_KONGO_JUNGLE_TINY_BATTLE
            || scene_minor == SCENE_ADVENTURE_MUSHROOM_KONGO_JUNGLE_GIANT_BATTLE
            || scene_minor == SCENE_ADVENTURE_UNDERGROUND_MAZE
            || scene_minor == SCENE_ADVENTURE_HYRULE_TEMPLE_BATTLE
            || scene_minor == SCENE_ADVENTURE_BRINSTAR
            || scene_minor == SCENE_ADVENTURE_ESCAPE_ZEBES
            || scene_minor == SCENE_ADVENTURE_GREEN_GREENS_KIRBY_BATTLE
            || scene_minor == SCENE_ADVENTURE_GREEN_GREENS_KIRBY_TEAM_BATTLE
            || scene_minor == SCENE_ADVENTURE_GREEN_GREENS_GIANT_KIRBY_BATTLE
            || scene_minor == SCENE_ADVENTURE_CORNERIA_BATTLE_1
            || scene_minor == SCENE_ADVENTURE_CORNERIA_BATTLE_2
            || scene_minor == SCENE_ADVENTURE_CORNERIA_BATTLE_3
            || scene_minor == SCENE_ADVENTURE_POKEMON_STADIUM_BATTLE
            || scene_minor == SCENE_ADVENTURE_FZERO_GRAND_PRIX_RACE
            || scene_minor == SCENE_ADVENTURE_FZERO_GRAND_PRIX_BATTLE
            || scene_minor == SCENE_ADVENTURE_ONETT_BATTLE
            || scene_minor == SCENE_ADVENTURE_ICICLE_MOUNTAIN_CLIMB
            || scene_minor == SCENE_ADVENTURE_BATTLEFIELD_BATTLE
            || scene_minor == SCENE_ADVENTURE_BATTLEFIELD_METAL_BATTLE
            || scene_minor == SCENE_ADVENTURE_FINAL_DESTINATION_BATTLE;
    }
    if scene_major == SCENE_TARGET_TEST {
        return scene_minor == SCENE_TARGET_TEST_INGAME;
    }
    if (SCENE_SUPER_SUDDEN_DEATH..=MENU_LIGHTNING_MELEE).contains(&scene_major) {
        return scene_minor == SCENE_SSD_INGAME;
    }
    if (SCENE_HOME_RUN_CONTEST..=SCENE_CRUEL_MELEE).contains(&scene_major) {
        return scene_minor == SCENE_HOME_RUN_CONTEST_INGAME;
    }
    if scene_major == SCENE_TITLE_SCREEN_IDLE {
        return scene_minor == SCENE_TITLE_SCREEN_IDLE_FIGHT_1 || scene_minor == SCENE_TITLE_SCREEN_IDLE_FIGHT_2;
    }

    false
}

/// Returns true if the player navigating the menus (including CSS and SSS)
/// Sourced from M'Overlay: https://github.com/bkacjios/m-overlay/blob/d8c629d/source/melee.lua#L1243
pub(crate) fn is_in_menus(scene_major: u8, scene_minor: u8) -> bool {
    if scene_major == SCENE_MAIN_MENU {
        return true;
    }
    if scene_major == SCENE_VS_MODE {
        return scene_minor == SCENE_VS_CSS || scene_minor == SCENE_VS_SSS;
    }
    if scene_major == SCENE_VS_ONLINE {
        return scene_minor == SCENE_VS_ONLINE_CSS || scene_minor == SCENE_VS_ONLINE_SSS || scene_minor == SCENE_VS_ONLINE_RANKED;
    }
    if (SCENE_TRAINING_MODE..=SCENE_STAMINA_MODE).contains(&scene_major) || scene_major == SCENE_FIXED_CAMERA_MODE {
        return scene_minor == SCENE_TRAINING_CSS || scene_minor == SCENE_TRAINING_SSS;
    }
    if scene_major == SCENE_EVENT_MATCH {
        return scene_minor == SCENE_EVENT_MATCH_SELECT;
    }
    if scene_major == SCENE_CLASSIC_MODE || scene_major == SCENE_ADVENTURE_MODE || scene_major == SCENE_ALL_STAR_MODE {
        return scene_minor == SCENE_CLASSIC_CSS;
    }
    if scene_major == SCENE_TARGET_TEST {
        return scene_minor == SCENE_TARGET_TEST_CSS;
    }
    if (SCENE_SUPER_SUDDEN_DEATH..=MENU_LIGHTNING_MELEE).contains(&scene_major) {
        return scene_minor == SCENE_SSD_CSS || scene_minor == SCENE_SSD_SSS;
    }
    if (SCENE_HOME_RUN_CONTEST..=SCENE_CRUEL_MELEE).contains(&scene_major) {
        return scene_minor == SCENE_HOME_RUN_CONTEST_CSS;
    }
    false
}
