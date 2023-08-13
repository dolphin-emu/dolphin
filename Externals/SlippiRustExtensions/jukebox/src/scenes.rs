/// Sourced from M'OVerlay: https://github.com/bkacjios/m-overlay/blob/d8c629d/source/melee.lua
#[rustfmt::skip]
#[allow(dead_code)]
pub(crate) mod scene_ids {
  pub(crate) const MATCH_NO_RESULT: u8 = 0x00;
  pub(crate) const MATCH_GAME: u8 = 0x02;
  pub(crate) const MATCH_STAGE_CLEAR: u8 = 0x03;
  pub(crate) const MATCH_STAGE_FAILURE: u8 = 0x04;
  pub(crate) const MATCH_STAGE_CLEAR3: u8 = 0x05;
  pub(crate) const MATCH_NEW_RECORD: u8 = 0x06;
  pub(crate) const MATCH_NO_CONTEST: u8 = 0x07;
  pub(crate) const MATCH_RETRY: u8 = 0x08;
  pub(crate) const MATCH_GAME_CLEAR: u8 = 0x09;

  // MAJOR FLAGS
  pub(crate) const SCENE_TITLE_SCREEN: u8 = 0x00;

  pub(crate) const SCENE_MAIN_MENU: u8 = 0x01;
    // MENU FLAGS
    pub(crate) const MENU_MAIN: u8 = 0x00;
      pub(crate) const SELECT_MAIN_1P: u8 = 0x00;
      pub(crate) const SELECT_MAIN_VS: u8 = 0x01;
      pub(crate) const SELECT_MAIN_TROPHY: u8 = 0x02;
      pub(crate) const SELECT_MAIN_OPTIONS: u8 = 0x03;
      pub(crate) const SELECT_MAIN_DATA: u8 = 0x04;

    pub(crate) const MENU_1P: u8 = 0x01;
      pub(crate) const SELECT_1P_REGULAR: u8 = 0x00;
      pub(crate) const SELECT_1P_EVENT: u8 = 0x01;
      pub(crate) const SELECT_1P_ONLINE: u8 = 0x2;
      pub(crate) const SELECT_1P_STADIUM: u8 = 0x03;
      pub(crate) const SELECT_1P_TRAINING: u8 = 0x04;

    pub(crate) const MENU_VS: u8 = 0x02;
      pub(crate) const SELECT_VS_MELEE: u8 = 0x00;
      pub(crate) const SELECT_VS_TOURNAMENT: u8 = 0x01;
      pub(crate) const SELECT_VS_SPECIAL: u8 = 0x02;
      pub(crate) const SELECT_VS_CUSTOM: u8 = 0x03;
      pub(crate) const SELECT_VS_NAMEENTRY: u8 = 0x04;

    pub(crate) const MENU_TROPHIES: u8 = 0x03;
      pub(crate) const SELECT_TROPHIES_GALLERY: u8 = 0x00;
      pub(crate) const SELECT_TROPHIES_LOTTERY: u8 = 0x01;
      pub(crate) const SELECT_TROPHIES_COLLECTION: u8 = 0x02;

    pub(crate) const MENU_OPTIONS: u8 = 0x04;
      pub(crate) const SELECT_OPTIONS_RUMBLE: u8 = 0x00;
      pub(crate) const SELECT_OPTIONS_SOUND: u8 = 0x01;
      pub(crate) const SELECT_OPTIONS_DISPLAY: u8 = 0x02;
      pub(crate) const SELECT_OPTIONS_UNKNOWN: u8 = 0x03;
      pub(crate) const SELECT_OPTIONS_LANGUAGE: u8 = 0x04;
      pub(crate) const SELECT_OPTIONS_ERASE_DATA: u8 = 0x05;

    pub(crate) const MENU_ONLINE: u8 = 0x08;
      pub(crate) const SELECT_ONLINE_RANKED: u8 = 0x00;
      pub(crate) const SELECT_ONLINE_UNRANKED: u8 = 0x01;
      pub(crate) const SELECT_ONLINE_DIRECT: u8 = 0x02;
      pub(crate) const SELECT_ONLINE_TEAMS: u8 = 0x03;
      pub(crate) const SELECT_ONLINE_LOGOUT: u8 = 0x05;

    pub(crate) const MENU_STADIUM: u8 = 0x09;
      pub(crate) const SELECT_STADIUM_TARGET_TEST: u8 = 0x00;
      pub(crate) const SELECT_STADIUM_HOMERUN_CONTEST: u8 = 0x01;
      pub(crate) const SELECT_STADIUM_MULTIMAN_MELEE: u8 = 0x02;

    pub(crate) const MENU_RUMBLE: u8 = 0x13;
    pub(crate) const MENU_SOUND: u8 = 0x14;
    pub(crate) const MENU_DISPLAY: u8 = 0x15;
    pub(crate) const MENU_UNKNOWN1: u8 = 0x16;
    pub(crate) const MENU_LANGUAGE: u8 = 0x17;

  pub(crate) const SCENE_VS_MODE: u8 = 0x02;
    // MINOR FLAGS
    pub(crate) const SCENE_VS_CSS: u8 = 0x0;
    pub(crate) const SCENE_VS_SSS: u8 = 0x1;
    pub(crate) const SCENE_VS_INGAME: u8 = 0x2;
    pub(crate) const SCENE_VS_POSTGAME: u8 = 0x4;

  pub(crate) const SCENE_CLASSIC_MODE: u8 = 0x03;
    pub(crate) const SCENE_CLASSIC_LEVEL_1_VS: u8  = 0x00;
    pub(crate) const SCENE_CLASSIC_LEVEL_1: u8 = 0x01;
    pub(crate) const SCENE_CLASSIC_LEVEL_2_VS: u8 = 0x02;
    pub(crate) const SCENE_CLASSIC_LEVEL_2: u8 = 0x03;
    pub(crate) const SCENE_CLASSIC_LEVEL_3_VS: u8 = 0x04;
    pub(crate) const SCENE_CLASSIC_LEVEL_3: u8 = 0x05;
    pub(crate) const SCENE_CLASSIC_LEVEL_4_VS: u8 = 0x06;
    pub(crate) const SCENE_CLASSIC_LEVEL_4: u8 = 0x07;
    // pub(crate) const SCENE_CLASSIC_LEVEL_5_VS: u8 = 0x08;
    // pub(crate) const SCENE_CLASSIC_LEVEL_5: u8 = 0x09;
    pub(crate) const SCENE_CLASSIC_LEVEL_5_VS: u8 = 0x10;
    pub(crate) const SCENE_CLASSIC_LEVEL_5: u8 = 0x09;

    pub(crate) const SCENE_CLASSIC_LEVEL_16: u8 = 0x20;
    pub(crate) const SCENE_CLASSIC_LEVEL_16_VS: u8 = 0x21;

    pub(crate) const SCENE_CLASSIC_LEVEL_24: u8 = 0x30;
    pub(crate) const SCENE_CLASSIC_LEVEL_24_VS: u8 = 0x31;

    pub(crate) const SCENE_CLASSIC_BREAK_THE_TARGETS_INTRO: u8 = 0x16;
    pub(crate) const SCENE_CLASSIC_BREAK_THE_TARGETS: u8 = 0x17;

    pub(crate) const SCENE_CLASSIC_TROPHY_STAGE_INTRO: u8 = 0x28;
    pub(crate) const SCENE_CLASSIC_TROPHY_STAGE_TARGETS: u8 = 0x29;

    pub(crate) const SCENE_CLASSIC_RACE_TO_FINISH_INTRO: u8 = 0x40;
    pub(crate) const SCENE_CLASSIC_RACE_TO_FINISH_TARGETS: u8 = 0x41;

    pub(crate) const SCENE_CLASSIC_LEVEL_56: u8 = 0x38;
    pub(crate) const SCENE_CLASSIC_LEVEL_56_VS: u8 = 0x39;

    pub(crate) const SCENE_CLASSIC_MASTER_HAND: u8 = 0x51;

    pub(crate) const SCENE_CLASSIC_CONTINUE: u8 = 0x69;
    pub(crate) const SCENE_CLASSIC_CSS: u8 = 0x70;

  pub(crate) const SCENE_ADVENTURE_MODE: u8 = 0x04;

    pub(crate) const SCENE_ADVENTURE_MUSHROOM_KINGDOM_INTRO: u8 = 0x00;
    pub(crate) const SCENE_ADVENTURE_MUSHROOM_KINGDOM: u8 = 0x01;
    pub(crate) const SCENE_ADVENTURE_MUSHROOM_KINGDOM_LUIGI: u8 = 0x02;
    pub(crate) const SCENE_ADVENTURE_MUSHROOM_KINGDOM_BATTLE: u8 = 0x03;

    pub(crate) const SCENE_ADVENTURE_MUSHROOM_KONGO_JUNGLE_INTRO: u8 = 0x08;
    pub(crate) const SCENE_ADVENTURE_MUSHROOM_KONGO_JUNGLE_TINY_BATTLE: u8 = 0x09;
    pub(crate) const SCENE_ADVENTURE_MUSHROOM_KONGO_JUNGLE_GIANT_BATTLE: u8 = 0x0A;

    pub(crate) const SCENE_ADVENTURE_UNDERGROUND_MAZE_INTRO: u8 = 0x10;
    pub(crate) const SCENE_ADVENTURE_UNDERGROUND_MAZE: u8 = 0x11;
    pub(crate) const SCENE_ADVENTURE_HYRULE_TEMPLE_BATTLE: u8 = 0x12;

    pub(crate) const SCENE_ADVENTURE_BRINSTAR_INTRO: u8 = 0x18;
    pub(crate) const SCENE_ADVENTURE_BRINSTAR: u8 = 0x19;

    pub(crate) const SCENE_ADVENTURE_ESCAPE_ZEBES_INTRO: u8 = 0x1A;
    pub(crate) const SCENE_ADVENTURE_ESCAPE_ZEBES: u8 = 0x1B;
    pub(crate) const SCENE_ADVENTURE_ESCAPE_ZEBES_ESCAPE: u8 = 0x1C;

    pub(crate) const SCENE_ADVENTURE_GREEN_GREENS_INTRO: u8 = 0x20;
    pub(crate) const SCENE_ADVENTURE_GREEN_GREENS_KIRBY_BATTLE: u8 = 0x21;
    pub(crate) const SCENE_ADVENTURE_GREEN_GREENS_KIRBY_TEAM_INTRO: u8 = 0x22;
    pub(crate) const SCENE_ADVENTURE_GREEN_GREENS_KIRBY_TEAM_BATTLE: u8 = 0x23;
    pub(crate) const SCENE_ADVENTURE_GREEN_GREENS_GIANT_KIRBY_INTRO: u8 = 0x24;
    pub(crate) const SCENE_ADVENTURE_GREEN_GREENS_GIANT_KIRBY_BATTLE: u8 = 0x25;

    pub(crate) const SCENE_ADVENTURE_CORNERIA_INTRO: u8 = 0x28;
    pub(crate) const SCENE_ADVENTURE_CORNERIA_BATTLE_1: u8 = 0x29;
    pub(crate) const SCENE_ADVENTURE_CORNERIA_RAID: u8 = 0x2A;
    pub(crate) const SCENE_ADVENTURE_CORNERIA_BATTLE_2: u8 = 0x2B;
    pub(crate) const SCENE_ADVENTURE_CORNERIA_BATTLE_3: u8 = 0x2C;

    pub(crate) const SCENE_ADVENTURE_POKEMON_STADIUM_INTRO: u8 = 0x30;
    pub(crate) const SCENE_ADVENTURE_POKEMON_STADIUM_BATTLE: u8 = 0x31;

    pub(crate) const SCENE_ADVENTURE_FZERO_GRAND_PRIX_CARS: u8 = 0x38;
    pub(crate) const SCENE_ADVENTURE_FZERO_GRAND_PRIX_INTRO: u8 = 0x39;
    pub(crate) const SCENE_ADVENTURE_FZERO_GRAND_PRIX_RACE: u8 = 0x3A;
    pub(crate) const SCENE_ADVENTURE_FZERO_GRAND_PRIX_BATTLE: u8 = 0x3B;

    pub(crate) const SCENE_ADVENTURE_ONETT_INTRO: u8 = 0x40;
    pub(crate) const SCENE_ADVENTURE_ONETT_BATTLE: u8 = 0x41;

    pub(crate) const SCENE_ADVENTURE_ICICLE_MOUNTAIN_INTRO: u8 = 0x48;
    pub(crate) const SCENE_ADVENTURE_ICICLE_MOUNTAIN_CLIMB: u8 = 0x49;

    pub(crate) const SCENE_ADVENTURE_BATTLEFIELD_INTRO: u8 = 0x50;
    pub(crate) const SCENE_ADVENTURE_BATTLEFIELD_BATTLE: u8 = 0x51;
    pub(crate) const SCENE_ADVENTURE_BATTLEFIELD_METAL_INTRO: u8 = 0x52;
    pub(crate) const SCENE_ADVENTURE_BATTLEFIELD_METAL_BATTLE: u8 = 0x53;

    pub(crate) const SCENE_ADVENTURE_FINAL_DESTINATION_INTRO: u8 = 0x58;
    pub(crate) const SCENE_ADVENTURE_FINAL_DESTINATION_BATTLE: u8 = 0x59;
    pub(crate) const SCENE_ADVENTURE_FINAL_DESTINATION_POSE: u8 = 0x5A;
    pub(crate) const SCENE_ADVENTURE_FINAL_DESTINATION_WINNER: u8 = 0x5B;

    pub(crate) const SCENE_ADVENTURE_CSS: u8 = 0x70;

  pub(crate) const SCENE_ALL_STAR_MODE: u8 = 0x05;
    pub(crate) const SCENE_ALL_STAR_LEVEL_1: u8 = 0x00;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_1: u8 = 0x01;
    pub(crate) const SCENE_ALL_STAR_LEVEL_2: u8 = 0x02;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_2: u8 = 0x03;
    pub(crate) const SCENE_ALL_STAR_LEVEL_3: u8 = 0x04;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_3: u8 = 0x05;
    pub(crate) const SCENE_ALL_STAR_LEVEL_4: u8 = 0x06;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_4: u8 = 0x07;
    pub(crate) const SCENE_ALL_STAR_LEVEL_5: u8 = 0x08;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_5: u8 = 0x09;
    pub(crate) const SCENE_ALL_STAR_LEVEL_6: u8 = 0x10;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_6: u8 = 0x11;
    pub(crate) const SCENE_ALL_STAR_LEVEL_7: u8 = 0x12;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_7: u8 = 0x13;
    pub(crate) const SCENE_ALL_STAR_LEVEL_8: u8 = 0x14;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_8: u8 = 0x15;
    pub(crate) const SCENE_ALL_STAR_LEVEL_9: u8 = 0x16;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_9: u8 = 0x17;
    pub(crate) const SCENE_ALL_STAR_LEVEL_10: u8 = 0x18;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_10: u8 = 0x19;
    pub(crate) const SCENE_ALL_STAR_LEVEL_11: u8 = 0x20;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_11: u8 = 0x21;
    pub(crate) const SCENE_ALL_STAR_LEVEL_12: u8 = 0x22;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_12: u8 = 0x23;
    pub(crate) const SCENE_ALL_STAR_LEVEL_13: u8 = 0x24;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_13: u8 = 0x25;
    pub(crate) const SCENE_ALL_STAR_LEVEL_14: u8 = 0x26;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_14: u8 = 0x27;
    pub(crate) const SCENE_ALL_STAR_LEVEL_15: u8 = 0x28;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_15: u8 = 0x29;
    pub(crate) const SCENE_ALL_STAR_LEVEL_16: u8 = 0x30;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_16: u8 = 0x31;
    pub(crate) const SCENE_ALL_STAR_LEVEL_17: u8 = 0x32;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_17: u8 = 0x33;
    pub(crate) const SCENE_ALL_STAR_LEVEL_18: u8 = 0x34;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_18: u8 = 0x35;
    pub(crate) const SCENE_ALL_STAR_LEVEL_19: u8 = 0x36;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_19: u8 = 0x37;
    pub(crate) const SCENE_ALL_STAR_LEVEL_20: u8 = 0x38;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_20: u8 = 0x39;
    pub(crate) const SCENE_ALL_STAR_LEVEL_21: u8 = 0x40;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_21: u8 = 0x41;
    pub(crate) const SCENE_ALL_STAR_LEVEL_22: u8 = 0x42;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_22: u8 = 0x43;
    pub(crate) const SCENE_ALL_STAR_LEVEL_23: u8 = 0x44;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_23: u8 = 0x45;
    pub(crate) const SCENE_ALL_STAR_LEVEL_24: u8 = 0x46;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_24: u8 = 0x47;
    pub(crate) const SCENE_ALL_STAR_LEVEL_25: u8 = 0x48;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_25: u8 = 0x49;
    pub(crate) const SCENE_ALL_STAR_LEVEL_26: u8 = 0x50;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_26: u8 = 0x51;
    pub(crate) const SCENE_ALL_STAR_LEVEL_27: u8 = 0x52;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_28: u8 = 0x53;
    pub(crate) const SCENE_ALL_STAR_LEVEL_29: u8 = 0x54;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_29: u8 = 0x55;
    pub(crate) const SCENE_ALL_STAR_LEVEL_30: u8 = 0x56;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_30: u8 = 0x57;
    pub(crate) const SCENE_ALL_STAR_LEVEL_31: u8 = 0x58;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_31: u8 = 0x59;
    pub(crate) const SCENE_ALL_STAR_LEVEL_32: u8 = 0x60;
    pub(crate) const SCENE_ALL_STAR_REST_AREA_32: u8 = 0x61;
    pub(crate) const SCENE_ALL_STAR_CSS: u8 = 0x70;

  pub(crate) const SCENE_DEBUG: u8 = 0x06;
  pub(crate) const SCENE_SOUND_TEST: u8 = 0x07;

  pub(crate) const SCENE_VS_ONLINE: u8 = 0x08; // SLIPPI ONLINE
    pub(crate) const SCENE_VS_ONLINE_CSS: u8 = 0x00;
    pub(crate) const SCENE_VS_ONLINE_SSS: u8 = 0x01;
    pub(crate) const SCENE_VS_ONLINE_INGAME: u8 = 0x02;
    pub(crate) const SCENE_VS_ONLINE_VERSUS: u8 = 0x04;
    pub(crate) const SCENE_VS_ONLINE_RANKED: u8 = 0x05;

  pub(crate) const SCENE_UNKOWN_1: u8 = 0x09;
  pub(crate) const SCENE_CAMERA_MODE: u8 = 0x0A;
  pub(crate) const SCENE_TROPHY_GALLERY: u8 = 0x0B;
  pub(crate) const SCENE_TROPHY_LOTTERY: u8 = 0x0C;
  pub(crate) const SCENE_TROPHY_COLLECTION: u8 = 0x0D;

  pub(crate) const SCENE_START_MATCH: u8 = 0x0E; // Slippi Replays
    pub(crate) const SCENE_START_MATCH_INGAME: u8 = 0x01; // Set when the replay is actually playing out
    pub(crate) const SCENE_START_MATCH_UNKNOWN: u8 = 0x03; // Seems to be set right before the match loads

  pub(crate) const SCENE_TARGET_TEST: u8 = 0x0F;
    pub(crate) const SCENE_TARGET_TEST_CSS: u8 = 0x00;
    pub(crate) const SCENE_TARGET_TEST_INGAME: u8 = 0x1;

  pub(crate) const SCENE_SUPER_SUDDEN_DEATH: u8 = 0x10;
    pub(crate) const SCENE_SSD_CSS: u8 = 0x00;
    pub(crate) const SCENE_SSD_SSS: u8 = 0x01;
    pub(crate) const SCENE_SSD_INGAME: u8 = 0x02;
    pub(crate) const SCENE_SSD_POSTGAME: u8 = 0x04;

  pub(crate) const MENU_INVISIBLE_MELEE: u8 = 0x11;
    pub(crate) const MENU_INVISIBLE_MELEE_CSS: u8 = 0x00;
    pub(crate) const MENU_INVISIBLE_MELEE_SSS: u8 = 0x01;
    pub(crate) const MENU_INVISIBLE_MELEE_INGAME: u8 = 0x02;
    pub(crate) const MENU_INVISIBLE_MELEE_POSTGAME: u8 = 0x04;

  pub(crate) const MENU_SLOW_MO_MELEE: u8 = 0x12;
    pub(crate) const MENU_SLOW_MO_MELEE_CSS: u8 = 0x00;
    pub(crate) const MENU_SLOW_MO_MELEE_SSS: u8 = 0x01;
    pub(crate) const MENU_SLOW_MO_MELEE_INGAME: u8 = 0x02;
    pub(crate) const MENU_SLOW_MO_MELEE_POSTGAME: u8 = 0x04;

  pub(crate) const MENU_LIGHTNING_MELEE: u8 = 0x13;
    pub(crate) const MENU_LIGHTNING_MELEE_CSS: u8 = 0x00;
    pub(crate) const MENU_LIGHTNING_MELEE_SSS: u8 = 0x01;
    pub(crate) const MENU_LIGHTNING_MELEE_INGAME: u8 = 0x02;
    pub(crate) const MENU_LIGHTNING_MELEE_POSTGAME: u8 = 0x04;

  pub(crate) const SCENE_CHARACTER_APPROACHING: u8 = 0x14;

  pub(crate) const SCENE_CLASSIC_MODE_COMPLETE: u8 = 0x15;
    pub(crate) const SCENE_CLASSIC_MODE_TROPHY: u8 = 0x00;
    pub(crate) const SCENE_CLASSIC_MODE_CREDITS: u8 = 0x01;
    pub(crate) const SCENE_CLASSIC_MODE_CHARACTER_VIDEO: u8 = 0x02;
    pub(crate) const SCENE_CLASSIC_MODE_CONGRATS: u8 = 0x03;

  pub(crate) const SCENE_ADVENTURE_MODE_COMPLETE: u8 = 0x16;
    pub(crate) const SCENE_ADVENTURE_MODE_TROPHY: u8 = 0x00;
    pub(crate) const SCENE_ADVENTURE_MODE_CREDITS: u8 = 0x01;
    pub(crate) const SCENE_ADVENTURE_MODE_CHARACTER_VIDEO: u8 = 0x02;
    pub(crate) const SCENE_ADVENTURE_MODE_CONGRATS: u8 = 0x03;

  pub(crate) const SCENE_ALL_STAR_COMPLETE: u8 = 0x17;
    pub(crate) const SCENE_ALL_STAR_TROPHY: u8 = 0x00;
    pub(crate) const SCENE_ALL_STAR_CREDITS: u8 = 0x01;
    pub(crate) const SCENE_ALL_STAR_CHARACTER_VIDEO: u8 = 0x02;
    pub(crate) const SCENE_ALL_STAR_CONGRATS: u8 = 0x03;

  pub(crate) const SCENE_TITLE_SCREEN_IDLE: u8 = 0x18;
    pub(crate) const SCENE_TITLE_SCREEN_IDLE_INTRO_VIDEO: u8 = 0x0;
    pub(crate) const SCENE_TITLE_SCREEN_IDLE_FIGHT_1: u8 = 0x1;
    pub(crate) const SCENE_TITLE_SCREEN_IDLE_BETWEEN_FIGHTS: u8 = 0x2;
    pub(crate) const SCENE_TITLE_SCREEN_IDLE_FIGHT_2: u8 = 0x3;
    pub(crate) const SCENE_TITLE_SCREEN_IDLE_HOW_TO_PLAY: u8 = 0x4;

  pub(crate) const SCENE_ADVENTURE_MODE_CINEMEATIC: u8 = 0x19;
  pub(crate) const SCENE_CHARACTER_UNLOCKED: u8 = 0x1A;

  pub(crate) const SCENE_TOURNAMENT: u8 = 0x1B;
    pub(crate) const SCENE_TOURNAMENT_CSS: u8 = 0x0;
    pub(crate) const SCENE_TOURNAMENT_BRACKET: u8 = 0x1;
    pub(crate) const SCENE_TOURNAMENT_INGAME: u8 = 0x4;
    pub(crate) const SCENE_TOURNAMENT_POSTGAME: u8 = 0x6;

  pub(crate) const SCENE_TRAINING_MODE: u8 = 0x1C;
    pub(crate) const SCENE_TRAINING_CSS: u8 = 0x0;
    pub(crate) const SCENE_TRAINING_SSS: u8 = 0x1;
    pub(crate) const SCENE_TRAINING_INGAME: u8 = 0x2;

  pub(crate) const SCENE_TINY_MELEE: u8 = 0x1D;
    pub(crate) const SCENE_TINY_MELEE_CSS: u8 = 0x0;
    pub(crate) const SCENE_TINY_MELEE_SSS: u8 = 0x1;
    pub(crate) const SCENE_TINY_MELEE_INGAME: u8 = 0x2;
    pub(crate) const SCENE_TINY_MELEE_POSTGAME: u8 = 0x4;

  pub(crate) const SCENE_GIANT_MELEE: u8 = 0x1E;
    pub(crate) const SCENE_GIANT_MELEE_CSS: u8 = 0x0;
    pub(crate) const SCENE_GIANT_MELEE_SSS: u8 = 0x1;
    pub(crate) const SCENE_GIANT_MELEE_INGAME: u8 = 0x2;
    pub(crate) const SCENE_GIANT_MELEE_POSTGAME: u8 = 0x4;

  pub(crate) const SCENE_STAMINA_MODE: u8 = 0x1F;
    pub(crate) const SCENE_STAMINA_MODE_CSS: u8 = 0x0;
    pub(crate) const SCENE_STAMINA_MODE_SSS: u8 = 0x1;
    pub(crate) const SCENE_STAMINA_MODE_INGAME: u8 = 0x2;
    pub(crate) const SCENE_STAMINA_MODE_POSTGAME: u8 = 0x4;

  pub(crate) const SCENE_HOME_RUN_CONTEST: u8 = 0x20;
    pub(crate) const SCENE_HOME_RUN_CONTEST_CSS: u8 = 0x0;
    pub(crate) const SCENE_HOME_RUN_CONTEST_INGAME: u8 = 0x1;

  pub(crate) const SCENE_10_MAN_MELEE: u8 = 0x21;
    pub(crate) const SCENE_10_MAN_MELEE_CSS: u8 = 0x00;
    pub(crate) const SCENE_10_MAN_MELEE_INGAME: u8 = 0x01;

  pub(crate) const SCENE_100_MAN_MELEE: u8 = 0x22;
    pub(crate) const SCENE_100_MAN_MELEE_CSS: u8 = 0x00;
    pub(crate) const SCENE_100_MAN_MELEE_INGAME: u8 = 0x01;

  pub(crate) const SCENE_3_MINUTE_MELEE: u8 = 0x23;
    pub(crate) const SCENE_3_MINUTE_MELEE_CSS: u8 = 0x00;
    pub(crate) const SCENE_3_MINUTE_MELEE_INGAME: u8 = 0x01;

  pub(crate) const SCENE_15_MINUTE_MELEE: u8 = 0x24;
    pub(crate) const SCENE_15_MINUTE_MELEE_CSS: u8 = 0x00;
    pub(crate) const SCENE_15_MINUTE_MELEE_INGAME: u8 = 0x01;

  pub(crate) const SCENE_ENDLESS_MELEE: u8 = 0x25;
    pub(crate) const SCENE_ENDLESS_MELEE_CSS: u8 = 0x00;
    pub(crate) const SCENE_ENDLESS_MELEE_INGAME: u8 = 0x01;

  pub(crate) const SCENE_CRUEL_MELEE: u8 = 0x26;
    pub(crate) const SCENE_CRUEL_MELEE_CSS: u8 = 0x00;
    pub(crate) const SCENE_CRUEL_MELEE_INGAME: u8 = 0x01;
    
  pub(crate) const SCENE_PROGRESSIVE_SCAN: u8 = 0x27;
  pub(crate) const SCENE_PLAY_INTRO_VIDEO: u8 = 0x28;
  pub(crate) const SCENE_MEMORY_CARD_OVERWRITE: u8 = 0x29;

  pub(crate) const SCENE_FIXED_CAMERA_MODE: u8 = 0x2A;
    pub(crate) const SCENE_FIXED_CAMERA_MODE_CSS: u8 = 0x0;
    pub(crate) const SCENE_FIXED_CAMERA_MODE_SSS: u8 = 0x1;
    pub(crate) const SCENE_FIXED_CAMERA_MODE_INGAME: u8 = 0x2;
    pub(crate) const SCENE_FIXED_CAMERA_MODE_POSTGAME: u8 = 0x4;

  pub(crate) const SCENE_EVENT_MATCH: u8 = 0x2B;
    pub(crate) const SCENE_EVENT_MATCH_SELECT: u8 = 0x0;
    pub(crate) const SCENE_EVENT_MATCH_INGAME: u8 = 0x1;

  pub(crate) const SCENE_SINGLE_BUTTON_MODE: u8 = 0x2C;
    pub(crate) const SCENE_SINGLE_BUTTON_MODE_CSS: u8 = 0x0;
    pub(crate) const SCENE_SINGLE_BUTTON_MODE_SSS: u8 = 0x1;
    pub(crate) const SCENE_SINGLE_BUTTON_MODE_INGAME: u8 = 0x2;

}
