// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _NETPLAY_PROTO_H
#define _NETPLAY_PROTO_H

#include "Common.h"
#include "CommonTypes.h"

struct NetSettings
{
	bool m_CPUthread;
	bool m_DSPHLE;
	bool m_DSPEnableJIT;
	bool m_WriteToMemcard;
};

struct Rpt : public std::vector<u8>
{
	u16		channel;
};

typedef std::vector<Rpt>	NetWiimote;

#define NETPLAY_VERSION		"Dolphin NetPlay 2013-08-23"

// messages
enum
{
	NP_MSG_PLAYER_JOIN		= 0x10,
	NP_MSG_PLAYER_LEAVE		= 0x11,

	NP_MSG_CHAT_MESSAGE		= 0x30,

	NP_MSG_PAD_DATA			= 0x60,
	NP_MSG_PAD_MAPPING		= 0x61,
	NP_MSG_PAD_BUFFER		= 0x62,

	NP_MSG_WIIMOTE_DATA		= 0x70,
	NP_MSG_WIIMOTE_MAPPING		= 0x71,	// just using pad mapping for now

	NP_MSG_START_GAME		= 0xA0,
	NP_MSG_CHANGE_GAME		= 0xA1,
	NP_MSG_STOP_GAME		= 0xA2,
	NP_MSG_DISABLE_GAME		= 0xA3,

	NP_MSG_READY			= 0xD0,
	NP_MSG_NOT_READY		= 0xD1,

	NP_MSG_PING			= 0xE0,
	NP_MSG_PONG			= 0xE1,
	NP_MSG_PLAYER_PING_DATA		= 0xE2,
};

typedef u8	MessageId;
typedef u8	PlayerId;
typedef s8	PadMapping;
typedef u32	FrameNum;

enum
{
	CON_ERR_SERVER_FULL = 1,
	CON_ERR_GAME_RUNNING,
	CON_ERR_VERSION_MISMATCH
};

#endif
