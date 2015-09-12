#include <iostream>
#include <sstream>
#include <string.h>
#include <thread>

#include "Common/Common.h"
#include "Common/GekkoDisassembler.h"
#include "Common/Thread.h"

#include "Core/Host.h"
#include "Core/PowerPC/JitInterface.h"

#include "UICommon/DebugInterface.h"
#include "UICommon/Disassembler.h"
std::unique_ptr<DebugAPI::DebugHandler> m_client;

// Stupid things
void Host_NotifyMapLoaded() {}
void Host_RefreshDSPDebuggerWindow() {}
void Host_Message(int Id) {}

void* Host_GetRenderHandle() { return NULL; }

void Host_UpdateTitle(const std::string& title) {}

void Host_UpdateDisasmDialog(){}

void Host_UpdateMainFrame() {}

void Host_RequestRenderWindowSize(int width, int height) {}

void Host_RequestFullscreen(bool enable_fullscreen) {}

void Host_SetStartupDebuggingParameters()
{
}

bool Host_UIHasFocus()
{
	return true;
}

bool Host_RendererHasFocus()
{
	return true;
}

bool Host_RendererIsFullscreen()
{
	return false;
}

void Host_ConnectWiimote(int wm_idx, bool connect) {}

void Host_SetWiiMoteConnectionState(int _State) {}

void Host_ShowVideoConfig(void*, const std::string&, const std::string&) {}

static bool s_running = true;
std::unique_ptr<HostDisassembler> m_disassembler;

enum ClientCommand
{
	CCMD_GETMAGIC = 0,
	CCMD_SETCORESTATE,
	CCMD_GETCORESTATE,
	CCMD_SETPROFILE,
	CCMD_GETPROFILERESULTS,
	CCMD_GETRAWCODE,
	CCMD_GETARCH,
	CCMD_SETCOREFEATURE,
	CCMD_GETCOREFEATURES,
	CCMD_HELP,
};

const std::vector<std::pair<std::string, u32>> command_list =
{
	{"SetState",   CCMD_SETCORESTATE},
	{"GetState",   CCMD_GETCORESTATE},
	{"SetProfile", CCMD_SETPROFILE},
	{"GetProfile", CCMD_GETPROFILERESULTS},
	{"DumpCode",   CCMD_GETRAWCODE},
	{"GetArch",    CCMD_GETARCH},
	{"SetCore",    CCMD_SETCOREFEATURE},
	{"GetCore",    CCMD_GETCOREFEATURES},
	{"Help",       CCMD_HELP},
};

static u32 GetCommandID(const std::string& cmd)
{

	std::locale loc;
	for (auto& it : command_list)
		if (!strcasecmp(it.first.c_str(), cmd.c_str()))
			return it.second;

	return (u32)-1;
}

static bool HandleCommand(std::string& command)
{
	if (command.empty())
		return true;

	std::string cmd;
	std::istringstream buffer(command);

	buffer >> cmd;

	u32 cmd_id = GetCommandID(cmd);
	switch (cmd_id)
	{
	case CCMD_GETARCH:
	{
		std::string arch = m_client->GetArch();
		printf("Architecture is '%s'\n", arch.c_str());
		m_disassembler.reset(GetNewDisassembler(arch));
	}
	break;
	case CCMD_GETCORESTATE:
	{
		printf("Core state is: %d\n", m_client->GetCoreState());
	}
	break;
	case CCMD_SETCORESTATE:
	{
		u32 state;
		buffer >> state;
		m_client->SetCoreState(state);
	}
	break;
	case CCMD_SETPROFILE:
	{
		u32 enable;
		buffer >> enable;
		m_client->SetProfile(!!enable);
	}
	break;
	case CCMD_GETPROFILERESULTS:
	{
		m_client->GetProfileResults();
	}
	break;
	case CCMD_GETRAWCODE:
	{
		std::vector<u32> ppc_code;
		std::vector<u8> host_code;
		u64 host_addr;
		u32 host_inst_count;
		u32 guest_addr;
		buffer >> std::hex >> guest_addr;
		m_client->DumpCode(guest_addr, &ppc_code, &host_code, &host_addr);

		File::IOFile f("dump.txt", "w");
		File::IOFile raw("dump.bin", "w");
		fprintf (f.GetHandle(), "Codesize for address 0x%08x is %ld bytes\n", guest_addr, host_code.size());


		std::string host_instructions_disasm;
		if (m_disassembler)
			host_instructions_disasm = m_disassembler->DisassembleHostBlock(&host_code[0], host_code.size(), &host_inst_count, host_addr);
		else
			printf("Haven't gotten host architecture yet!\n");

		fprintf(f.GetHandle(), "%s\n", host_instructions_disasm.c_str());
		fprintf(f.GetHandle(), "Guest codesize is %ld bytes\n", ppc_code.size());
		std::ostringstream ppc_disasm;
		for (u32 i = 0; i < ppc_code.size(); ++i)
		{
			u32 inst = ppc_code[i];
			std::string opcode = GekkoDisassembler::Disassemble(inst, guest_addr + i * 4);
			ppc_disasm << std::setfill('0') << std::setw(8) << std::hex << (guest_addr + i * 4);
			ppc_disasm << " " << opcode << std::endl;
		}
		fprintf(f.GetHandle(), "%s", ppc_disasm.str().c_str());
	}
	break;
	case CCMD_SETCOREFEATURE:
	{
		std::string disable;
		std::string feature;
		u32 disable_var = 0;
		buffer >> disable;
		buffer >> feature;
		std::array<std::string, 8> features =
		{
			"JIT",
			"JIT LoadStore",
			"JIT LoadStore Floating",
			"JIT LoadStore Paired",
			"JIT Floating Point",
			"JIT Integer",
			"JIT Paired",
			"JIT System Registers",
		};

		if (disable == "disable")
			disable_var = true;
		else
			disable_var = false;
		m_client->SetCoreFeature(feature, disable_var);
	}
	break;
	case CCMD_GETCOREFEATURES:
	{
		std::vector<std::pair<std::string, bool>> features = m_client->GetCoreFeatures();

		for (auto it : features)
			printf("%s:%s\n", it.second ? "disabled " : "enabled  ", it.first.c_str());
	}
	break;
	case CCMD_HELP:
		printf("Commands Available\n");
		for (auto& it : command_list)
			printf("\t%s\n", it.first.c_str());
	break;
	case 0xFFFFFFFF:
		goto err;
	break;
	}

	return true;
err:
	return false;
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf ("%s <ip>\n", argv[0]);
		return -2;
	}

	m_client = DebugAPI::InitClient(argv[1]);

	while (s_running)
	{
		std::string command;
		std::getline(std::cin, command);

		if (!HandleCommand(command))
			printf("Unknown command '%s'\n", command.c_str());
	}

	return 0;
}
