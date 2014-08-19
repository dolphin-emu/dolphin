// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/IniFile.h"

enum Hotkey
{
	HK_OPEN,
	HK_CHANGE_DISC,
	HK_REFRESH_LIST,

	HK_PLAY_PAUSE,
	HK_STOP,
	HK_RESET,
	HK_FRAME_ADVANCE,

	HK_START_RECORDING,
	HK_PLAY_RECORDING,
	HK_EXPORT_RECORDING,
	HK_READ_ONLY_MODE,

	HK_FULLSCREEN,
	HK_SCREENSHOT,
	HK_EXIT,

	HK_WIIMOTE1_CONNECT,
	HK_WIIMOTE2_CONNECT,
	HK_WIIMOTE3_CONNECT,
	HK_WIIMOTE4_CONNECT,
	HK_BALANCEBOARD_CONNECT,

	HK_TOGGLE_IR,
	HK_TOGGLE_AR,
	HK_TOGGLE_EFBCOPIES,
	HK_TOGGLE_FOG,
	HK_TOGGLE_THROTTLE,

	HK_INCREASE_FRAME_LIMIT,
	HK_DECREASE_FRAME_LIMIT,

	HK_LOAD_STATE_SLOT_1,
	HK_LOAD_STATE_SLOT_2,
	HK_LOAD_STATE_SLOT_3,
	HK_LOAD_STATE_SLOT_4,
	HK_LOAD_STATE_SLOT_5,
	HK_LOAD_STATE_SLOT_6,
	HK_LOAD_STATE_SLOT_7,
	HK_LOAD_STATE_SLOT_8,
	HK_LOAD_STATE_SLOT_9,
	HK_LOAD_STATE_SLOT_10,

	HK_SAVE_STATE_SLOT_1,
	HK_SAVE_STATE_SLOT_2,
	HK_SAVE_STATE_SLOT_3,
	HK_SAVE_STATE_SLOT_4,
	HK_SAVE_STATE_SLOT_5,
	HK_SAVE_STATE_SLOT_6,
	HK_SAVE_STATE_SLOT_7,
	HK_SAVE_STATE_SLOT_8,
	HK_SAVE_STATE_SLOT_9,
	HK_SAVE_STATE_SLOT_10,

	HK_LOAD_LAST_STATE_1,
	HK_LOAD_LAST_STATE_2,
	HK_LOAD_LAST_STATE_3,
	HK_LOAD_LAST_STATE_4,
	HK_LOAD_LAST_STATE_5,
	HK_LOAD_LAST_STATE_6,
	HK_LOAD_LAST_STATE_7,
	HK_LOAD_LAST_STATE_8,

	HK_SAVE_FIRST_STATE,
	HK_UNDO_LOAD_STATE,
	HK_UNDO_SAVE_STATE,
	HK_SAVE_STATE_FILE,
	HK_LOAD_STATE_FILE,

	NUM_HOTKEYS,
};

struct CoreStartupParameter
{
	// Settings
	bool m_enable_debugging;
	#ifdef USE_GDBSTUB
	int  m_GDB_port;
	#endif
	bool m_automatic_start;
	bool m_boot_to_pause;

	// 0 = Interpreter
	// 1 = Jit
	// 2 = JitIL
	// 3 = JIT ARM
	int m_CPU_core;

	// JIT (shared between JIT and JITIL)
	bool m_JIT_no_block_cache, m_JIT_block_linking;
	bool m_JIT_off;
	bool m_JIT_load_store_off, m_JIT_load_store_lxz_off, m_JIT_load_store_lwz_off, m_JIT_load_store_lbzx_off;
	bool m_JIT_load_store_floating_off;
	bool m_JIT_load_store_paired_off;
	bool m_JIT_floating_point_off;
	bool m_JIT_integer_off;
	bool m_JIT_paired_off;
	bool m_JIT_system_registers_off;
	bool m_JIT_branch_off;
	bool m_JITIL_time_profiling;
	bool m_JITIL_output_IR;

	bool m_fastmem;
	bool m_enable_FPRF;

	bool m_CPU_thread;
	bool m_DSP_thread;
	bool m_DSPHLE;
	bool m_skip_idle;
	bool m_NTSC;
	bool m_force_NTSCJ;
	bool m_HLE_BS2;
	bool m_enable_cheats;
	bool m_merge_blocks;
	bool m_enable_memcard_saving;

	bool m_DPL2_decoder;
	int m_latency;

	bool m_run_compare_server;
	bool m_run_compare_client;

	bool m_MMU;
	bool m_DCBZOFF;
	bool m_TLB_hack;
	int m_BB_dump_port;
	bool m_vbeam_speed_hack;
	bool m_sync_GPU;
	bool m_fast_disc_speed;

	int m_selected_language;

	bool m_wii;

	// Interface settings
	bool m_confirm_stop, m_hide_cursor, m_auto_hide_cursor, m_use_panic_handlers, m_on_screen_display_messages;
	std::string m_theme_name;

	// Hotkeys
	int m_hotkey[NUM_HOTKEYS];
	int m_hotkey_modifier[NUM_HOTKEYS];

	// Display settings
	std::string m_fullscreen_resolution;
	int m_render_window_xpos, m_render_window_ypos;
	int m_render_window_width, m_render_window_height;
	bool m_render_window_auto_size, m_keep_window_on_top;
	bool m_fullscreen, m_render_to_main;
	bool m_progressive, m_disable_screen_saver;

	int m_posx, m_posy, m_width, m_height;

	// Fifo Player related settings
	bool m_loop_fifo_replay;

	enum BootBS2
	{
		BOOT_DEFAULT,
		BOOT_BS2_JAP,
		BOOT_BS2_USA,
		BOOT_BS2_EUR,
	};

	enum BootType
	{
		BOOT_ISO,
		BOOT_ELF,
		BOOT_DOL,
		BOOT_WII_NAND,
		BOOT_BS2,
		BOOT_DFF
	};
	BootType m_boot_type;

	std::string m_video_backend;

	// files

	std::string m_filename;
	std::string m_boot_ROM;
	std::string m_SRAM;
	std::string m_default_GCM;
	std::string m_DVD_root;
	std::string m_apploader;
	std::string m_unique_ID;
	std::string m_revision_specific_unique_ID;
	std::string m_name;
	std::string m_game_ini_default;
	std::string m_game_ini_default_revision_specific;
	std::string m_game_ini_local;

	// Constructor just calls LoadDefaults
	CoreStartupParameter();

	void LoadDefaults();
	bool AutoSetup(BootBS2 boot_BS2);
	const std::string &GetUniqueID() const { return m_unique_ID; }
	void CheckMemcardPath(std::string& memcard_path, std::string region, bool is_slot_a);
	IniFile LoadDefaultGameIni() const;
	IniFile LoadLocalGameIni() const;
	IniFile LoadGameIni() const;
};
