// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "LibDolphin.h"

#ifdef _WIN32

#include <Windows.h>
#include "Core/HW/Memmap.h"
#include <InputCommon/GCPadStatus.h>
#include <InputCommon/InputConfig.h>
#include "Core/HW/GCPad.h"

bool m_accepting_connections;
bool m_connected;

GCPad *emu_pad;

HANDLE h_pipe;

void LibDolphin::Init() {
	m_accepting_connections = true;

	h_pipe = CreateNamedPipe(
		LPCTSTR(PIPE_NAME),
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE |
		PIPE_NOWAIT,
		1,
		16,
		16,
		0,
		NULL
	);
};

void LibDolphin::AcceptConnections(bool accept) {
	m_accepting_connections = accept;
}

void LibDolphin::RunConnection() {
	if (!m_connected) {
		char buffer[2];
		DWORD bytes;
		ReadFile(
			h_pipe,
			buffer,
			2,
			&bytes,
			NULL
		);

		if ((bytes == 2) && (buffer[0] == 0x0B)) {
			char buffer[2];
			buffer[0] = 0x1B;
			DWORD bytes;
			WriteFile(
				h_pipe,
				buffer,
				2,
				&bytes,
				NULL
			);

			emu_pad = Pad::InitLibDolphinVirt();

			m_connected = true;
		}

		return;
	}
	else {
		while (true) {
			char buffer[1];
			DWORD bytes;
			ReadFile(
				h_pipe,
				buffer,
				1,
				&bytes,
				NULL
			);

			u32 address;

			switch (buffer[0]) {
				// Read8
			case 0x01:
			{
				char abuffer[4];
				ReadFile(
					h_pipe,
					abuffer,
					4,
					&bytes,
					NULL
					);
				address = ((u32)abuffer[0] << 24) + ((u32)abuffer[1] << 16) + ((u32)abuffer[2] << 8) + ((u32)abuffer[3]);
				u8 value = Memory::Read_U8(address);

				char rbuffer[3];
				rbuffer[0] = 0x11;
				rbuffer[1] = (char)value;

				WriteFile(
					h_pipe,
					rbuffer,
					3,
					&bytes,
					NULL
					);

				break;
			}

				// Read16
			case 0x02:
			{
				char abuffer[4];
				ReadFile(
					h_pipe,
					abuffer,
					4,
					&bytes,
					NULL
					);
				address = ((u32)abuffer[0] << 24) + ((u32)abuffer[1] << 16) + ((u32)abuffer[2] << 8) + ((u32)abuffer[3]);
				u16 value = Memory::Read_U16(address);

				u8 rvalue1 = ((value | 0xFF00) >> 8);
				u8 rvalue2 = ((value | 0x00FF));

				char rbuffer[4];
				rbuffer[0] = 0x12;
				rbuffer[1] = (char)rvalue1;
				rbuffer[2] = (char)rvalue2;

				WriteFile(
					h_pipe,
					rbuffer,
					4,
					&bytes,
					NULL
					);

				break;
			}

				// Read32
			case 0x03:
			{
				char abuffer[4];
				ReadFile(
					h_pipe,
					abuffer,
					4,
					&bytes,
					NULL
					);
				const u32 address = ((u32)abuffer[0] << 24) + ((u32)abuffer[1] << 16) + ((u32)abuffer[2] << 8) + ((u32)abuffer[3]);
				u32 value = Memory::Read_U32(address);

				u8 rvalue1 = ((value | 0xFF000000) >> 24);
				u8 rvalue2 = ((value | 0x00FF0000) >> 16);
				u8 rvalue3 = ((value | 0x0000FF00) >> 8);
				u8 rvalue4 = ((value | 0x0000000FF));

				char rbuffer[6];
				rbuffer[0] = 0x13;
				rbuffer[1] = (char)rvalue1;
				rbuffer[2] = (char)rvalue2;
				rbuffer[3] = (char)rvalue3;
				rbuffer[4] = (char)rvalue4;

				WriteFile(
					h_pipe,
					rbuffer,
					6,
					&bytes,
					NULL
					);
				break;
			}

				// Read64
			case 0x04:
			{
				char abuffer[4];
				ReadFile(
					h_pipe,
					abuffer,
					4,
					&bytes,
					NULL
					);
				const u32 address = ((u32)abuffer[0] << 24) + ((u32)abuffer[1] << 16) + ((u32)abuffer[2] << 8) + ((u32)abuffer[3]);
				u64 value = Memory::Read_U64(address);

				u8 rvalue1 = (u8)((value | 0xFF00000000000000) >> 56);
				u8 rvalue2 = (u8)((value | 0x00FF000000000000) >> 48);
				u8 rvalue3 = (u8)((value | 0x0000FF0000000000) >> 40);
				u8 rvalue4 = (u8)((value | 0x000000FF00000000) >> 32);
				u8 rvalue5 = (u8)((value | 0x00000000FF000000) >> 24);
				u8 rvalue6 = (u8)((value | 0x0000000000FF0000) >> 16);
				u8 rvalue7 = (u8)((value | 0x000000000000FF00) >> 8);
				u8 rvalue8 = (u8)((value | 0x0000000000000FF));

				char rbuffer[10];
				rbuffer[0] = 0x14;
				rbuffer[1] = (char)rvalue1;
				rbuffer[2] = (char)rvalue2;
				rbuffer[3] = (char)rvalue3;
				rbuffer[4] = (char)rvalue4;
				rbuffer[5] = (char)rvalue5;
				rbuffer[6] = (char)rvalue6;
				rbuffer[7] = (char)rvalue7;
				rbuffer[8] = (char)rvalue8;

				WriteFile(
					h_pipe,
					rbuffer,
					10,
					&bytes,
					NULL
					);

				break;
			}

				// Write8
			case 0x05:
			{
				char abuffer[4];
				ReadFile(
					h_pipe,
					abuffer,
					4,
					&bytes,
					NULL
					);
				const u32 address = ((u32)abuffer[0] << 24) + ((u32)abuffer[1] << 16) + ((u32)abuffer[2] << 8) + ((u32)abuffer[3]);

				char vbuffer[1];
				ReadFile(
					h_pipe,
					vbuffer,
					1,
					&bytes,
					NULL
					);

				const u8 value = (const u8)vbuffer[0];
				Memory::Write_U8(value, address);

				char rbuffer[2];
				rbuffer[0] = 0x15;

				WriteFile(
					h_pipe,
					rbuffer,
					2,
					&bytes,
					NULL
					);

				break;
			}

				// Write16
			case 0x06:
			{
				char abuffer[4];
				ReadFile(
					h_pipe,
					abuffer,
					4,
					&bytes,
					NULL
					);
				const u32 address = ((u32)abuffer[0] << 24) + ((u32)abuffer[1] << 16) + ((u32)abuffer[2] << 8) + ((u32)abuffer[3]);

				char vbuffer[2];
				ReadFile(
					h_pipe,
					vbuffer,
					2,
					&bytes,
					NULL
					);

				const u16 value = (const u16)(((u16)vbuffer[0] << 8) + ((u16)vbuffer[1]));
				Memory::Write_U16(value, address);

				char rbuffer[2];
				rbuffer[0] = 0x16;

				WriteFile(
					h_pipe,
					rbuffer,
					2,
					&bytes,
					NULL
					);
				break;
			}

				// Write32
			case 0x07:
			{
				char abuffer[4];
				ReadFile(
					h_pipe,
					abuffer,
					4,
					&bytes,
					NULL
					);
				const u32 address = ((u32)abuffer[0] << 24) + ((u32)abuffer[1] << 16) + ((u32)abuffer[2] << 8) + ((u32)abuffer[3]);

				char vbuffer[4];
				ReadFile(
					h_pipe,
					vbuffer,
					4,
					&bytes,
					NULL
					);

				const u32 value = (const u32)(((u32)vbuffer[0] << 24) + ((u32)vbuffer[1] << 16) + ((u32)vbuffer[2] << 8) + ((u32)vbuffer[3]));
				Memory::Write_U32(value, address);

				char rbuffer[2];
				rbuffer[0] = 0x17;

				WriteFile(
					h_pipe,
					rbuffer,
					2,
					&bytes,
					NULL
					);
				break;
			}

				// Write64
			case 0x08:
			{
				char abuffer[4];
				ReadFile(
					h_pipe,
					abuffer,
					4,
					&bytes,
					NULL
					);
				const u32 address = ((u32)abuffer[0] << 24) + ((u32)abuffer[1] << 16) + ((u32)abuffer[2] << 8) + ((u32)abuffer[3]);

				char vbuffer[8];
				ReadFile(
					h_pipe,
					vbuffer,
					8,
					&bytes,
					NULL
					);

				const u64 value = (const u64)(((u64)vbuffer[0] << 56) + ((u64)vbuffer[1] << 48) + ((u64)vbuffer[2] << 40) + ((u64)vbuffer[3] << 32) + ((u64)vbuffer[4] << 24) + ((u64)vbuffer[5] << 16) + ((u64)vbuffer[6] << 8) + ((u64)vbuffer[7]));
				Memory::Write_U64(value, address);

				char rbuffer[2];
				rbuffer[0] = 0x18;

				WriteFile(
					h_pipe,
					rbuffer,
					2,
					&bytes,
					NULL
					);
				break;
			}

				// QueueInput
			case 0x09:
			{
				char ibuffer[16];
				ReadFile(
					h_pipe,
					ibuffer,
					16,
					&bytes,
					NULL
					);
				u8 input[16];

				input[0] = ibuffer[0];
				input[1] = ibuffer[1];
				input[2] = ibuffer[2];
				input[3] = ibuffer[3];
				input[4] = ibuffer[4];
				input[5] = ibuffer[5];
				input[6] = ibuffer[6];
				input[7] = ibuffer[7];
				input[8] = ibuffer[8];
				input[9] = ibuffer[9];
				input[10] = ibuffer[10];
				input[11] = ibuffer[11];
				input[12] = ibuffer[12];
				input[13] = ibuffer[13];
				input[14] = ibuffer[14];
				input[15] = ibuffer[15];

				struct GCPadStatus pad;
				pad.button = ((u16)input[0] << 8) + ((u16)input[1]);
				pad.stickX = input[2];
				pad.stickY = input[3];
				pad.substickX = input[4];
				pad.substickY = input[5];
				pad.triggerLeft = input[6];
				pad.triggerRight = input[7];

				pad.analogA = input[8];
				pad.analogB = input[9];
				pad.err = 0;

				emu_pad->SetEmulatedInput(pad);

				break;
			}

				// Frame advance
			case 0x0A:
			{
				// I know it's bad practice, but we have to break from the while loop.
				goto finish;
			}

			default:
				continue;

			}
		}

	finish:
		return;
	}
}

#else

void LibDolphin::Init() {}

void LibDolphin::AcceptConnections(bool accept) {}

void RunConnection() {}

#endif