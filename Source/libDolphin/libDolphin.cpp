// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This is the library to communicate with Dolphin.

#include "Stdafx.h"
#include "libDolphin.h"
#include <Windows.h>

struct GCPadStatus m_current_pad;
void(*m_callback) ();

HANDLE h_pipe;

bool Dolphin::Init() {
	h_pipe = CreateFile(
		LPCWSTR(PIPE_NAME),
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	char buffer[2];
	DWORD bytes;
	buffer[0] = 0x0B;

	WriteFile(
		h_pipe,
		buffer,
		2,
		&bytes,
		NULL
	);

	ReadFile(
		h_pipe,
		buffer,
		2,
		&bytes,
		NULL
	);

	if ((bytes != 2) || (buffer[0] != 0x1B)) {
		return false;
	}

	return true;
};

void Dolphin::SetFrameCallback(void(*callback)()) {
	m_callback = callback;
};

void RunFrameCallback(void(*callback) ()) {
	(*callback)();
}

// Potential return values
// -1: Didn't set the frame callback.
// -2: Invalid response from server.
int Dolphin::StartLoop() {
	if (m_callback == NULL) {
		return -1;
	}

	while (true) {
		m_current_pad = EMPTY_CONTROL;

		RunFrameCallback(m_callback);
		u64 *input;

		input = InterpretInput(m_current_pad);

		char *buffer;

		buffer[0] = 0x09;

		u64 *ubuffer = (u64*) buffer + 1;

		ubuffer[0] = input[0];
		ubuffer[1] = input[1];

		char rbuffer[2];
		DWORD bytes;

		WriteFile(
			h_pipe,
			buffer,
			18,
			&bytes,
			NULL
		);

		ReadFile(
			h_pipe,
			rbuffer,
			2,
			&bytes,
			NULL
		);

		if ((bytes != 2) || (rbuffer[0] != 0x19)) {
			return -2;
		}

		char buffer2[2];

		buffer2[0] = 0x0A;

		WriteFile(
			h_pipe,
			buffer2,
			2,
			&bytes,
			NULL
		);

		ReadFile(
			h_pipe,                // handle to pipe 
			buffer,             // buffer to receive data 
			2,     // size of buffer 
			&bytes,             // number of bytes read 
			NULL
		);

		if ((bytes != 2) || (buffer[0] != 0x1A)) {
			return -2;
		}
	}
}

u64* Dolphin::InterpretInput(struct GCPadStatus pad) {
	u64* input;

	u64 input1;
	u64 input2;

	// Create a u64 to represent input.
	// Uses the hardcoded values of GCPadStatus, and assumes no error.
	input1 = ((u64)pad.button << 48) + ((u64)pad.stickX << 40) + ((u64)pad.stickY << 32) + ((u64)pad.substickX << 24) + ((u64)pad.substickY << 16) + ((u64)pad.triggerLeft << 8) + ((u64)pad.triggerRight);
	input2 = ((u64)pad.analogA << 56) + ((u64)pad.analogB << 48) + 0x80807f80807f;

	input[0] = input1;
	input[1] = input2;

	return input;
}

void Dolphin::QueueInput(struct GCPadStatus pad) {
	m_current_pad = pad;
}

u8 Dolphin::Read8(u32 address) {
	char *buffer;
	buffer[0] = 0x01;

	u32 *ubuffer = (u32*)buffer + 1;
	ubuffer[0] = address;

	DWORD bytes;

	WriteFile(
		h_pipe,
		buffer,
		6,
		&bytes,
		NULL
	);

	char rbuffer[3];
	ReadFile(
		h_pipe,
		rbuffer,
		3,
		&bytes,
		NULL
	);

	if ((bytes != 3) || (rbuffer[0] != 0x11)) {
		return 0;
	}

	return (u8)rbuffer[1];
}

u16 Dolphin::Read16(u32 address) {
	char *buffer;
	buffer[0] = 0x02;

	u32 *ubuffer = (u32*)buffer + 1;
	ubuffer[0] = address;

	DWORD bytes;

	WriteFile(
		h_pipe,
		buffer,
		6,
		&bytes,
		NULL
		);

	char rbuffer[4];
	ReadFile(
		h_pipe,
		rbuffer,
		4,
		&bytes,
		NULL
		);

	if ((bytes != 4) || (rbuffer[0] != 0x12)) {
		return 0;
	}

	return ((u16)rbuffer[1] << 8) + ((u16)rbuffer[2]);
}

u32 Dolphin::Read32(u32 address) {
	char *buffer;
	buffer[0] = 0x03;

	u32 *ubuffer = (u32*)buffer + 1;
	ubuffer[0] = address;

	DWORD bytes;

	WriteFile(
		h_pipe,
		buffer,
		6,
		&bytes,
		NULL
		);

	char rbuffer[6];
	ReadFile(
		h_pipe,
		rbuffer,
		6,
		&bytes,
		NULL
		);

	if ((bytes != 6) || (rbuffer[0] != 0x13)) {
		return 0;
	}

	return ((u16)rbuffer[1] << 24) + ((u16)rbuffer[2] << 16) + ((u16)rbuffer[3] << 8) + ((u16)rbuffer[4]);
}

u64 Dolphin::Read64(u32 address) {
	char *buffer;
	buffer[0] = 0x04;

	u32 *ubuffer = (u32*)buffer + 1;
	ubuffer[0] = address;

	DWORD bytes;

	WriteFile(
		h_pipe,
		buffer,
		6,
		&bytes,
		NULL
		);

	char rbuffer[10];
	ReadFile(
		h_pipe,
		rbuffer,
		6,
		&bytes,
		NULL
		);

	if ((bytes != 10) || (rbuffer[0] != 0x14)) {
		return 0;
	}

	return ((u16)rbuffer[1] << 56) + ((u16)rbuffer[2] << 48) + ((u16)rbuffer[3] << 40) + ((u16)rbuffer[4] << 32) + ((u16)rbuffer[5] << 24) + ((u16)rbuffer[6] << 16) + ((u16)rbuffer[7] << 8) + ((u16)rbuffer[8]);
}

void Dolphin::Write8(u32 address, u8 value) {
	char *buffer;
	buffer[0] = 0x05;

	u32 *ubuffer = (u32*)buffer + 1;
	ubuffer[0] = address;

	u8 *vbuffer = (u8*)buffer + 5;
	vbuffer[0] = value;

	DWORD bytes;

	WriteFile(
		h_pipe,
		buffer,
		7,
		&bytes,
		NULL
		);

	char rbuffer[2];
	ReadFile(
		h_pipe,
		rbuffer,
		2,
		&bytes,
		NULL
		);

	if ((bytes != 2) || (rbuffer[0] != 0x15)) {
		return;
	}

	return;
}

void Dolphin::Write16(u32 address, u16 value) {
	char *buffer;
	buffer[0] = 0x06;

	u32 *ubuffer = (u32*)buffer + 1;
	ubuffer[0] = address;

	u16 *vbuffer = (u16*)buffer + 5;
	vbuffer[0] = value;

	DWORD bytes;

	WriteFile(
		h_pipe,
		buffer,
		8,
		&bytes,
		NULL
		);

	char rbuffer[2];
	ReadFile(
		h_pipe,
		rbuffer,
		2,
		&bytes,
		NULL
		);

	if ((bytes != 2) || (rbuffer[0] != 0x16)) {
		return;
	}

	return;
}

void Dolphin::Write32(u32 address, u32 value) {
	char *buffer;
	buffer[0] = 0x07;

	u32 *ubuffer = (u32*)buffer + 1;
	ubuffer[0] = address;

	u32 *vbuffer = (u32*)buffer + 5;
	vbuffer[0] = value;

	DWORD bytes;

	WriteFile(
		h_pipe,
		buffer,
		10,
		&bytes,
		NULL
		);

	char rbuffer[2];
	ReadFile(
		h_pipe,
		rbuffer,
		2,
		&bytes,
		NULL
		);

	if ((bytes != 2) || (rbuffer[0] != 0x17)) {
		return;
	}

	return;
}

void Dolphin::Write64(u32 address, u64 value) {
	char *buffer;
	buffer[0] = 0x08;

	u32 *ubuffer = (u32*)buffer + 1;
	ubuffer[0] = address;

	u64 *vbuffer = (u64*)buffer + 5;
	vbuffer[0] = value;

	DWORD bytes;

	WriteFile(
		h_pipe,
		buffer,
		14,
		&bytes,
		NULL
		);

	char rbuffer[2];
	ReadFile(
		h_pipe,
		rbuffer,
		2,
		&bytes,
		NULL
		);

	if ((bytes != 2) || (rbuffer[0] != 0x18)) {
		return;
	}

	return;
}