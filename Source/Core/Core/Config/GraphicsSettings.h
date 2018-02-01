// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Config/Config.h"

namespace Config
{
// Configuration Information

// Graphics.Hardware

extern const ConfigInfo<bool> GFX_VSYNC;
extern const ConfigInfo<int> GFX_ADAPTER;

// Graphics.Settings

extern const ConfigInfo<bool> GFX_WIDESCREEN_HACK;
extern const ConfigInfo<int> GFX_ASPECT_RATIO;
extern const ConfigInfo<int> GFX_SUGGESTED_ASPECT_RATIO;
extern const ConfigInfo<bool> GFX_CROP;
extern const ConfigInfo<bool> GFX_USE_XFB;
extern const ConfigInfo<bool> GFX_USE_REAL_XFB;
extern const ConfigInfo<int> GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES;
extern const ConfigInfo<bool> GFX_SHOW_FPS;
extern const ConfigInfo<bool> GFX_SHOW_NETPLAY_PING;
extern const ConfigInfo<bool> GFX_SHOW_NETPLAY_MESSAGES;
extern const ConfigInfo<bool> GFX_LOG_RENDER_TIME_TO_FILE;
extern const ConfigInfo<bool> GFX_OVERLAY_STATS;
extern const ConfigInfo<bool> GFX_OVERLAY_PROJ_STATS;
extern const ConfigInfo<bool> GFX_DUMP_TEXTURES;
extern const ConfigInfo<bool> GFX_HIRES_TEXTURES;
extern const ConfigInfo<bool> GFX_CONVERT_HIRES_TEXTURES;
extern const ConfigInfo<bool> GFX_CACHE_HIRES_TEXTURES;
extern const ConfigInfo<bool> GFX_DUMP_EFB_TARGET;
extern const ConfigInfo<bool> GFX_DUMP_FRAMES_AS_IMAGES;
extern const ConfigInfo<bool> GFX_FREE_LOOK;
extern const ConfigInfo<bool> GFX_USE_FFV1;
extern const ConfigInfo<std::string> GFX_DUMP_FORMAT;
extern const ConfigInfo<std::string> GFX_DUMP_CODEC;
extern const ConfigInfo<std::string> GFX_DUMP_PATH;
extern const ConfigInfo<int> GFX_BITRATE_KBPS;
extern const ConfigInfo<bool> GFX_INTERNAL_RESOLUTION_FRAME_DUMPS;
extern const ConfigInfo<bool> GFX_ENABLE_GPU_TEXTURE_DECODING;
extern const ConfigInfo<bool> GFX_ENABLE_PIXEL_LIGHTING;
extern const ConfigInfo<bool> GFX_FAST_DEPTH_CALC;
extern const ConfigInfo<u32> GFX_MSAA;
extern const ConfigInfo<bool> GFX_SSAA;
extern const ConfigInfo<int> GFX_EFB_SCALE;
extern const ConfigInfo<int> GFX_INTERNAL_RESOLUTION;
extern const ConfigInfo<bool> GFX_TEXFMT_OVERLAY_ENABLE;
extern const ConfigInfo<bool> GFX_TEXFMT_OVERLAY_CENTER;
extern const ConfigInfo<bool> GFX_ENABLE_WIREFRAME;
extern const ConfigInfo<bool> GFX_DISABLE_FOG;
extern const ConfigInfo<bool> GFX_BORDERLESS_FULLSCREEN;
extern const ConfigInfo<bool> GFX_ENABLE_VALIDATION_LAYER;
extern const ConfigInfo<bool> GFX_BACKEND_MULTITHREADING;
extern const ConfigInfo<int> GFX_COMMAND_BUFFER_EXECUTE_INTERVAL;
extern const ConfigInfo<bool> GFX_SHADER_CACHE;
extern const ConfigInfo<bool> GFX_BACKGROUND_SHADER_COMPILING;
extern const ConfigInfo<bool> GFX_DISABLE_SPECIALIZED_SHADERS;
extern const ConfigInfo<bool> GFX_PRECOMPILE_UBER_SHADERS;
extern const ConfigInfo<int> GFX_SHADER_COMPILER_THREADS;
extern const ConfigInfo<int> GFX_SHADER_PRECOMPILER_THREADS;

extern const ConfigInfo<bool> GFX_SW_ZCOMPLOC;
extern const ConfigInfo<bool> GFX_SW_ZFREEZE;
extern const ConfigInfo<bool> GFX_SW_DUMP_OBJECTS;
extern const ConfigInfo<bool> GFX_SW_DUMP_TEV_STAGES;
extern const ConfigInfo<bool> GFX_SW_DUMP_TEV_TEX_FETCHES;
extern const ConfigInfo<int> GFX_SW_DRAW_START;
extern const ConfigInfo<int> GFX_SW_DRAW_END;

extern const ConfigInfo<bool> GFX_PREFER_GLES;

// Graphics.Enhancements

extern const ConfigInfo<bool> GFX_ENHANCE_FORCE_FILTERING;
extern const ConfigInfo<int> GFX_ENHANCE_MAX_ANISOTROPY;  // NOTE - this is x in (1 << x)
extern const ConfigInfo<std::string> GFX_ENHANCE_POST_SHADER;
extern const ConfigInfo<bool> GFX_ENHANCE_FORCE_TRUE_COLOR;

// Graphics.Stereoscopy

extern const ConfigInfo<int> GFX_STEREO_MODE;
extern const ConfigInfo<int> GFX_STEREO_DEPTH;
extern const ConfigInfo<int> GFX_STEREO_CONVERGENCE_PERCENTAGE;
extern const ConfigInfo<bool> GFX_STEREO_SWAP_EYES;
extern const ConfigInfo<int> GFX_STEREO_CONVERGENCE;
extern const ConfigInfo<bool> GFX_STEREO_EFB_MONO_DEPTH;
extern const ConfigInfo<int> GFX_STEREO_DEPTH_PERCENTAGE;

// Graphics.Hacks

extern const ConfigInfo<bool> GFX_HACK_EFB_ACCESS_ENABLE;
extern const ConfigInfo<bool> GFX_HACK_BBOX_ENABLE;
extern const ConfigInfo<bool> GFX_HACK_BBOX_PREFER_STENCIL_IMPLEMENTATION;
extern const ConfigInfo<bool> GFX_HACK_FORCE_PROGRESSIVE;
extern const ConfigInfo<bool> GFX_HACK_EFB_COPY_ENABLE;
extern const ConfigInfo<bool> GFX_HACK_EFB_COPY_CLEAR_DISABLE;
extern const ConfigInfo<bool> GFX_HACK_SKIP_EFB_COPY_TO_RAM;
extern const ConfigInfo<bool> GFX_HACK_COPY_EFB_ENABLED;
extern const ConfigInfo<bool> GFX_HACK_EFB_EMULATE_FORMAT_CHANGES;
extern const ConfigInfo<bool> GFX_HACK_VERTEX_ROUDING;

// Graphics.GameSpecific

extern const ConfigInfo<int> GFX_PROJECTION_HACK;
extern const ConfigInfo<int> GFX_PROJECTION_HACK_SZNEAR;
extern const ConfigInfo<int> GFX_PROJECTION_HACK_SZFAR;
extern const ConfigInfo<std::string> GFX_PROJECTION_HACK_ZNEAR;
extern const ConfigInfo<std::string> GFX_PROJECTION_HACK_ZFAR;
extern const ConfigInfo<bool> GFX_PERF_QUERIES_ENABLE;

// Graphics.Game Specific VR Settings
extern const ConfigInfo<bool> GFX_VR_DISABLE_3D;
extern const ConfigInfo<bool> GFX_VR_HUD_FULLSCREEN;
extern const ConfigInfo<bool> GFX_VR_HUD_ON_TOP;
extern const ConfigInfo<bool> GFX_VR_DONT_CLEAR_SCREEN;
extern const ConfigInfo<bool> GFX_VR_CAN_READ_CAMERA_ANGLES;
extern const ConfigInfo<bool> GFX_VR_DETECT_SKYBOX;
extern const ConfigInfo<float> GFX_VR_UNITS_PER_METRE;
extern const ConfigInfo<float> GFX_VR_HUD_THICKNESS;
extern const ConfigInfo<float> GFX_VR_HUD_DISTANCE;
extern const ConfigInfo<float> GFX_VR_HUD_3D_CLOSER;
extern const ConfigInfo<float> GFX_VR_CAMERA_FORWARD;
extern const ConfigInfo<float> GFX_VR_CAMERA_PITCH;
extern const ConfigInfo<float> GFX_VR_AIM_DISTANCE;
extern const ConfigInfo<float> GFX_VR_MIN_FOV;
extern const ConfigInfo<float> GFX_VR_N64_FOV;
extern const ConfigInfo<float> GFX_VR_SCREEN_HEIGHT;
extern const ConfigInfo<float> GFX_VR_SCREEN_THICKNESS;
extern const ConfigInfo<float> GFX_VR_SCREEN_DISTANCE;
extern const ConfigInfo<float> GFX_VR_SCREEN_RIGHT;
extern const ConfigInfo<float> GFX_VR_SCREEN_UP;
extern const ConfigInfo<float> GFX_VR_SCREEN_PITCH;
extern const ConfigInfo<int> GFX_VR_METROID_PRIME;
extern const ConfigInfo<int> GFX_VR_TELESCOPE_EYE;
extern const ConfigInfo<float> GFX_VR_TELESCOPE_MAX_FOV;
extern const ConfigInfo<float> GFX_VR_READ_PITCH;
extern const ConfigInfo<u32> GFX_VR_CAMERA_MIN_POLY;
extern const ConfigInfo<float> GFX_VR_HUD_DESP_POSITION_0;
extern const ConfigInfo<float> GFX_VR_HUD_DESP_POSITION_1;
extern const ConfigInfo<float> GFX_VR_HUD_DESP_POSITION_2;

// Global VR Settings
extern const ConfigInfo<float> GLOBAL_VR_SCALE;
extern const ConfigInfo<float> GLOBAL_VR_FREE_LOOK_SENSITIVITY;
extern const ConfigInfo<float> GLOBAL_VR_LEAN_BACK_ANGLE;
extern const ConfigInfo<bool> GLOBAL_VR_ENABLE_VR;
extern const ConfigInfo<bool> GLOBAL_VR_LOW_PERSISTENCE;
extern const ConfigInfo<bool> GLOBAL_VR_DYNAMIC_PREDICTION;
extern const ConfigInfo<bool> GLOBAL_VR_NO_MIRROR_TO_WINDOW;
extern const ConfigInfo<bool> GLOBAL_VR_ORIENTATION_TRACKING;
extern const ConfigInfo<bool> GLOBAL_VR_MAG_YAW_CORRECTION;
extern const ConfigInfo<bool> GLOBAL_VR_POSITION_TRACKING;
extern const ConfigInfo<bool> GLOBAL_VR_CHROMATIC;
extern const ConfigInfo<bool> GLOBAL_VR_TIMEWARP;
extern const ConfigInfo<bool> GLOBAL_VR_VIGNETTE;
extern const ConfigInfo<bool> GLOBAL_VR_NO_RESTORE;
extern const ConfigInfo<bool> GLOBAL_VR_FLIP_VERTICAL;
extern const ConfigInfo<bool> GLOBAL_VR_SRGB;
extern const ConfigInfo<bool> GLOBAL_VR_OVERDRIVE;
extern const ConfigInfo<bool> GLOBAL_VR_HQ_DISTORTION;
extern const ConfigInfo<bool> GLOBAL_VR_DISABLE_NEAR_CLIPPING;
extern const ConfigInfo<bool> GLOBAL_VR_AUTO_PAIR_VIVE_CONTROLLERS;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_HANDS;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_FEET;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_CONTROLLER;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_LASER_POINTER;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_AIM_RECTANGLE;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_HUD_BOX;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_2D_SCREEN_BOX;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_SENSOR_BAR;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_GAME_CAMERA;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_GAME_FRUSTUM;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_TRACKING_CAMERA;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_TRACKING_VOLUME;
extern const ConfigInfo<bool> GLOBAL_VR_SHOW_BASE_STATION;
extern const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_ALWAYS;
extern const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_FREELOOK;
extern const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_2D;
extern const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_LEFT_STICK;
extern const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_RIGHT_STICK;
extern const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_DPAD;
extern const ConfigInfo<bool> GLOBAL_VR_MOTION_SICKNESS_IR;
extern const ConfigInfo<int> GLOBAL_VR_MOTION_SICKNESS_METHOD;
extern const ConfigInfo<int> GLOBAL_VR_MOTION_SICKNESS_SKYBOX;
extern const ConfigInfo<float> GLOBAL_VR_MOTION_SICKNESS_FOV;
extern const ConfigInfo<int> GLOBAL_VR_PLAYER;
extern const ConfigInfo<int> GLOBAL_VR_PLAYER_2;
extern const ConfigInfo<int> GLOBAL_VR_MIRROR_PLAYER;
extern const ConfigInfo<int> GLOBAL_VR_MIRROR_STYLE;
extern const ConfigInfo<float> GLOBAL_VR_TIMEWARP_TWEAK;
extern const ConfigInfo<u32> GLOBAL_VR_NUM_EXTRA_FRAMES;
extern const ConfigInfo<u32> GLOBAL_VR_NUM_EXTRA_VIDEO_LOOPS;
extern const ConfigInfo<u32> GLOBAL_VR_NUM_EXTRA_VIDEO_LOOPS_DIVIDER;
extern const ConfigInfo<bool> GLOBAL_VR_STABILIZE_ROLL;
extern const ConfigInfo<bool> GLOBAL_VR_STABILIZE_PITCH;
extern const ConfigInfo<bool> GLOBAL_VR_STABILIZE_YAW;
extern const ConfigInfo<bool> GLOBAL_VR_STABILIZE_X;
extern const ConfigInfo<bool> GLOBAL_VR_STABILIZE_Y;
extern const ConfigInfo<bool> GLOBAL_VR_STABILIZE_Z;
extern const ConfigInfo<bool> GLOBAL_VR_KEYHOLE;
extern const ConfigInfo<float> GLOBAL_VR_KEYHOLE_WIDTH;
extern const ConfigInfo<bool> GLOBAL_VR_KEYHOLE_SNAP;
extern const ConfigInfo<float> GLOBAL_VR_KEYHOLE_SIZE;
extern const ConfigInfo<bool> GLOBAL_VR_PULL_UP_20_FPS;
extern const ConfigInfo<bool> GLOBAL_VR_PULL_UP_30_FPS;
extern const ConfigInfo<bool> GLOBAL_VR_PULL_UP_60_FPS;
extern const ConfigInfo<bool> GLOBAL_VR_PULL_UP_AUTO;
extern const ConfigInfo<bool> GLOBAL_VR_OPCODE_REPLAY;
extern const ConfigInfo<bool> GLOBAL_VR_OPCODE_WARNING_DISABLE;
extern const ConfigInfo<bool> GLOBAL_VR_REPLAY_VERTEX_DATA;
extern const ConfigInfo<bool> GLOBAL_VR_REPLAY_OTHER_DATA;
extern const ConfigInfo<bool> GLOBAL_VR_PULL_UP_20_FPS_TIMEWARP;
extern const ConfigInfo<bool> GLOBAL_VR_PULL_UP_30_FPS_TIMEWARP;
extern const ConfigInfo<bool> GLOBAL_VR_PULL_UP_60_FPS_TIMEWARP;
extern const ConfigInfo<bool> GLOBAL_VR_PULL_UP_AUTO_TIMEWARP;
extern const ConfigInfo<bool> GLOBAL_VR_SYNCHRONOUS_TIMEWARP;
extern const ConfigInfo<std::string> GLOBAL_VR_LEFT_TEXTURE;
extern const ConfigInfo<std::string> GLOBAL_VR_RIGHT_TEXTURE;
extern const ConfigInfo<std::string> GLOBAL_VR_GC_LEFT_TEXTURE;
extern const ConfigInfo<std::string> GLOBAL_VR_GC_RIGHT_TEXTURE;

}  // namespace Config
