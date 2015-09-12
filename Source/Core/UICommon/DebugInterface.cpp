// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <SFML/Network/Packet.hpp>

#include "Common/ENetUtil.h"
#include "Common/Event.h"

#include "Core/Core.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#include "UICommon/DebugInterface.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace DebugAPI
{
const u16 DEBUG_PORT = 45678;
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

auto S64Send = [](sf::Packet& pac, u64 val)
{
	union
	{
		struct
		{
			u32 low;
			u32 high;
		};
		u64 data;
	} data;
	data.data = val;
	pac << data.low;
	pac << data.high;
};

auto S64Get = [](sf::Packet& pac)
{
	union
	{
		struct
		{
			u32 low;
			u32 high;
		};
		u64 data;
	} data;
	pac >> data.low;
	pac >> data.high;
	return data.data;
};

class LocalDebugHandler : public DebugHandler
{
public:
	std::string GetArch()
	{
#if defined(_M_X86)
		return "x86";
#elif defined(_M_ARM_64)
		return "aarch64";
#else
		return "UNK";
#endif

	}
	u32 GetCoreState()
	{
		return Core::GetState();
	}
	void SetCoreState(u32 state)
	{
		switch (state)
		{
		case 0:
			Core::SetState(Core::CORE_RUN);
		break;
		case 1:
			Core::SetState(Core::CORE_PAUSE);
		break;
		}
	}
	std::vector<std::pair<std::string, bool>> GetCoreFeatures()
	{
		std::vector<std::pair<std::string, bool>> cpu_features;
		cpu_features.emplace_back(std::make_pair("JIT", SConfig::GetInstance().bJITOff));
		cpu_features.emplace_back(std::make_pair("LoadStore", SConfig::GetInstance().bJITLoadStoreOff));
		cpu_features.emplace_back(std::make_pair("LoadStoreFP", SConfig::GetInstance().bJITLoadStoreFloatingOff));
		cpu_features.emplace_back(std::make_pair("LoadStorePaired", SConfig::GetInstance().bJITLoadStorePairedOff));
		cpu_features.emplace_back(std::make_pair("Float", SConfig::GetInstance().bJITFloatingPointOff));
		cpu_features.emplace_back(std::make_pair("Integer", SConfig::GetInstance().bJITIntegerOff));
		cpu_features.emplace_back(std::make_pair("Paired", SConfig::GetInstance().bJITPairedOff));
		cpu_features.emplace_back(std::make_pair("System", SConfig::GetInstance().bJITSystemRegistersOff));
		return cpu_features;
	}
	void SetCoreFeature(std::string& core_part, bool disabled)
	{
		std::unordered_map<std::string, bool*> features = {
			{"jit",             &SConfig::GetInstance().bJITOff},
			{"loadstore",       &SConfig::GetInstance().bJITLoadStoreOff},
			{"loadstorefp",     &SConfig::GetInstance().bJITLoadStoreFloatingOff},
			{"loadstorepaired", &SConfig::GetInstance().bJITLoadStorePairedOff},
			{"float",           &SConfig::GetInstance().bJITFloatingPointOff},
			{"integer",         &SConfig::GetInstance().bJITIntegerOff},
			{"paired",          &SConfig::GetInstance().bJITPairedOff},
			{"system",          &SConfig::GetInstance().bJITSystemRegistersOff},
		};
		std::transform(core_part.begin(), core_part.end(), core_part.begin(), ::tolower);
		*features[core_part] = disabled;
	}
	void SetProfile(bool enable)
	{
		if (enable)
		{
			OSD::AddMessage("Enabling profiling");
			Core::SetState(Core::CORE_PAUSE);
			if (jit != nullptr)
				jit->ClearCache();
			Profiler::g_ProfileBlocks = true;
			Core::SetState(Core::CORE_RUN);
		}
		else
		{
			OSD::AddMessage("Disabling profiling");
			Core::SetState(Core::CORE_PAUSE);
			if (jit != nullptr)
				jit->ClearCache();
			Profiler::g_ProfileBlocks = false;
			Core::SetState(Core::CORE_RUN);
		}
	}
	void GetProfileResults()
	{
		Core::EState prev_state = Core::GetState();
		if (prev_state == Core::CORE_RUN)
			Core::SetState(Core::CORE_PAUSE);

		if (Core::GetState() == Core::CORE_PAUSE && PowerPC::GetMode() == PowerPC::MODE_JIT)
		{
			if (jit != nullptr)
			{
				std::string filename = File::GetUserPath(D_DUMP_IDX) + "Debug/profiler.txt";
				File::CreateFullPath(filename);
				ProfileStats prof_stats;
				JitInterface::GetProfileResults(&prof_stats);
				JitInterface::WriteProfileResults(prof_stats, filename);
			}
		}

		if (prev_state == Core::CORE_RUN)
			Core::SetState(Core::CORE_RUN);
	}
	void DumpCode(u32 address, std::vector<u32>* ppc, std::vector<u8>* host, u64* host_addr)
	{
		ppc->clear();
		host->clear();
		const u8* code;
		u32 host_code_size = 0;
		u32 guest_code_size = 0;

		// Host code
		JitInterface::GetHostCode(&address, &code, &host_code_size);

		// Guest code
		PPCAnalyst::CodeBuffer code_buffer(32000);
		PPCAnalyst::BlockStats st;
		PPCAnalyst::BlockRegStats gpa;
		PPCAnalyst::BlockRegStats fpa;
		PPCAnalyst::CodeBlock code_block;
		PPCAnalyst::PPCAnalyzer analyzer;
		analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);

		code_block.m_stats = &st;
		code_block.m_gpa = &gpa;
		code_block.m_fpa = &fpa;

		if (analyzer.Analyze(address, &code_block, &code_buffer, 32000) != 0xFFFFFFFF)
			guest_code_size = code_block.m_num_instructions;

		*host_addr = (u64)code;
		for (size_t i = 0; i < (guest_code_size / 4); ++i)
			ppc->emplace_back(code_buffer.codebuffer[i].inst.hex);

		for (size_t i = 0; i < host_code_size; ++i)
			host->emplace_back(code[i]);
	}
};

class ClientDebugHandler : public DebugHandler
{
private:
	bool m_inited;
	std::string m_ip;
	std::thread client_thread;
	bool m_running;

	// Connections
	ENetAddress m_address;
	ENetHost* m_client;
	ENetPeer* m_peer;
	sf::Packet rpac;
	Common::Event m_packet_available;
	Common::Event m_packet_handled;

public:
	void ClientThread()
	{
		int event_status;
		ENetEvent event;

		while (m_running)
		{
			event_status = enet_host_service(m_client, &event, 500);

			if (event_status > 0)
			{
				switch (event.type)
				{
				case ENET_EVENT_TYPE_CONNECT:
					WARN_LOG(COMMON, "(Client) We got a new connection from %x\n", event.peer->address.host);
				break;
				case ENET_EVENT_TYPE_RECEIVE:
				{
					rpac.append(event.packet->data, event.packet->dataLength);
					enet_packet_destroy(event.packet);
					m_packet_available.Set();
					m_packet_handled.Wait();
					rpac.clear();
				}
				break;
				case ENET_EVENT_TYPE_DISCONNECT:
					event.peer->data = NULL;
					m_running = false;
					m_inited = false;
					return;
				break;
				case ENET_EVENT_TYPE_NONE:
				break;
				}
			}
		}

	}
	void SendPacket(sf::Packet& pack)
	{
		ENetPacket* epac = enet_packet_create(pack.getData(), pack.getDataSize(), ENET_PACKET_FLAG_RELIABLE);
		enet_peer_send(m_peer, 0, epac);
	}
	void WaitForResponse()
	{
		m_packet_available.Wait();
	}
	void SetPacketHandled()
	{
		m_packet_handled.Set();
	}
	ClientDebugHandler(std::string ip)
		: m_ip(ip)
	{
		if (enet_initialize() != 0)
			goto cleanup;

		m_client = enet_host_create(NULL, 1, 2, 57600/8, 14400/8);

		if (!m_client)
		{
			ERROR_LOG(COMMON, "Couldn't create client host!\n");
			goto cleanup;
		}

		enet_address_set_host(&m_address, ip.c_str());
		m_address.port = DEBUG_PORT;

		m_peer = enet_host_connect(m_client, &m_address, 2, 0);

		m_inited = true;
		m_running = true;
		client_thread = std::thread(&ClientDebugHandler::ClientThread, this);
		return;
cleanup:
			m_inited = false;
			enet_deinitialize();

	}
	~ClientDebugHandler()
	{
		m_running = false;
		client_thread.join();

		enet_host_destroy(m_client);
		enet_deinitialize();
		m_inited = false;
	}
	bool IsAvailable()
	{
		return m_inited && m_running;
	}
	std::string GetArch()
	{
		sf::Packet pack;
		pack << CCMD_GETARCH;
		SendPacket(pack);
		WaitForResponse();
		std::string arch;
		rpac >> arch;
		SetPacketHandled();
		return arch;
	}
	u32 GetCoreState()
	{
		sf::Packet pack;
		pack << CCMD_GETCORESTATE;
		SendPacket(pack);
		WaitForResponse();
		u32 state;
		rpac >> state;
		SetPacketHandled();
		return state;
	}
	void SetCoreState(u32 state)
	{
		sf::Packet pack;
		pack << CCMD_SETCORESTATE;
		pack << state;
		SendPacket(pack);
	}
	std::vector<std::pair<std::string, bool>> GetCoreFeatures()
	{
		sf::Packet pack;
		pack << CCMD_GETCOREFEATURES;
		SendPacket(pack);
		WaitForResponse();
		std::vector<std::pair<std::string, bool>> features;
		std::array<std::string, 8> feature_strings =
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
		for (int i = 0; i < 8; ++i)
		{
			u32 disabled;
			rpac >> disabled;
			features.emplace_back(std::make_pair(feature_strings[i], !!disabled));
		}
		SetPacketHandled();
		return features;
	}
	void SetCoreFeature(std::string& core_part, bool disabled)
	{
		sf::Packet pack;
		pack << CCMD_SETCOREFEATURE;
		pack << core_part;
		pack << (u32)disabled;
		SendPacket(pack);
	}
	void SetProfile(bool enable)
	{
		sf::Packet pack;
		pack << CCMD_SETPROFILE;
		pack << (u32)enable;
		SendPacket(pack);
	}
	void GetProfileResults()
	{
		sf::Packet pack;
		pack << CCMD_GETPROFILERESULTS;
		SendPacket(pack);
		WaitForResponse();

		ProfileStats prof_stats;
		u64 blocks;
		prof_stats.cost_sum = S64Get(rpac);
		prof_stats.timecost_sum = S64Get(rpac);
		prof_stats.countsPerSec = S64Get(rpac);
		blocks = S64Get(rpac);

		printf ("We go %ld blocks\n", blocks);
		for (u64 i = 0; i < blocks; ++i)
		{
			int block_num;
			u32 addr;
			u64 cost;
			u64 ticks;
			u64 run_count;
			u32 block_size;
			rpac >> block_num;
			rpac >> addr;
			cost = S64Get(rpac);
			ticks = S64Get(rpac);
			run_count = S64Get(rpac);
			rpac >> block_size;
			prof_stats.block_stats.emplace_back(block_num, addr, cost, ticks, run_count, block_size);
		}
		SetPacketHandled();

		u64 highest_ticks = 0;
		u32 best_addr = 0;
		for (auto& it : prof_stats.block_stats)
			if (highest_ticks < it.tick_counter)
			{
				highest_ticks = it.tick_counter;
				best_addr = it.addr;
			}
		std::string filename = File::GetUserPath(D_DUMP_IDX) + "Debug/profiler.txt";
		File::CreateFullPath(filename);
		JitInterface::WriteProfileResults(prof_stats, filename);
		printf("Block taking the longest is 0x%08x\n", best_addr);
	}
	void DumpCode(u32 address, std::vector<u32>* ppc, std::vector<u8>* host, u64* host_addr)
	{
		sf::Packet pack;
		pack << CCMD_GETRAWCODE;
		pack << address;
		SendPacket(pack);
		WaitForResponse();

		u32 addr;
		u32 host_code_size;
		u32 guest_code_size;
		rpac >> addr;
		*host_addr = S64Get(rpac);
		rpac >> guest_code_size;
		rpac >> host_code_size;

		std::ostringstream ppc_disasm;
		for (u32 i = 0; i < (guest_code_size / 4); i++)
		{
			u32 inst;
			rpac >> inst;
			ppc->emplace_back(inst);
		}

		for (u32 i = 0; i < host_code_size; ++i)
		{
			u8 code_byte;
			rpac >> code_byte;
			host->emplace_back(code_byte);
		}
		SetPacketHandled();
	}
};

class DebugHandlerServer : public DebugHandlerServerBase
{
private:
	bool m_inited;
	bool m_running;

	std::thread m_thread;
	std::unique_ptr<ENetHost> m_server;

public:
	void HandlePacket(sf::Packet& packet, ENetPeer* peer)
	{
		bool send_response = false;
		sf::Packet response_pack;
		u32 com;

		packet >> com;

		switch (com)
		{
		case CCMD_GETMAGIC:
		break;
		case CCMD_SETCORESTATE:
		{
			u32 state;
			packet >> state;
			switch (state)
			{
			case 0:
				Core::SetState(Core::CORE_RUN);
			break;
			case 1:
				Core::SetState(Core::CORE_PAUSE);
			break;
			}
		}
		break;
		case CCMD_GETCORESTATE:
		{
			send_response = true;
			response_pack << Core::GetState();
		}
		break;
		case CCMD_SETPROFILE:
		{
			u32 enable;
			packet >> enable;
			if (enable)
			{
				OSD::AddMessage("Enabling profiling");
				Core::SetState(Core::CORE_PAUSE);
				if (jit != nullptr)
					jit->ClearCache();
				Profiler::g_ProfileBlocks = true;
				Core::SetState(Core::CORE_RUN);
			}
			else
			{
				OSD::AddMessage("Disabling profiling");
				Core::SetState(Core::CORE_PAUSE);
				if (jit != nullptr)
					jit->ClearCache();
				Profiler::g_ProfileBlocks = false;
				Core::SetState(Core::CORE_RUN);
			}
		}
		break;
		case CCMD_GETPROFILERESULTS:
		{
			ProfileStats prof_stats;
			JitInterface::GetProfileResults(&prof_stats);

			send_response = true;
			S64Send(response_pack, prof_stats.cost_sum);
			S64Send(response_pack, prof_stats.timecost_sum);
			S64Send(response_pack, prof_stats.countsPerSec);
			S64Send(response_pack, prof_stats.block_stats.size());

			for (auto& stat : prof_stats.block_stats)
			{
				response_pack << stat.blockNum;
				response_pack << stat.addr;
				S64Send(response_pack, stat.cost);
				S64Send(response_pack, stat.tick_counter);
				S64Send(response_pack, stat.run_count);
				response_pack << stat.block_size;
			}
		}
		break;
		case CCMD_GETRAWCODE:
		{
			u32 address;
			const u8* code;
			u32 host_code_size = 0;
			packet >> address;

			// Host code
			JitInterface::GetHostCode(&address, &code, &host_code_size);

			// Guest code
			PPCAnalyst::CodeBuffer code_buffer(32000);
			PPCAnalyst::BlockStats st;
			PPCAnalyst::BlockRegStats gpa;
			PPCAnalyst::BlockRegStats fpa;
			PPCAnalyst::CodeBlock code_block;
			PPCAnalyst::PPCAnalyzer analyzer;
			analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);

			code_block.m_stats = &st;
			code_block.m_gpa = &gpa;
			code_block.m_fpa = &fpa;

			analyzer.Analyze(address, &code_block, &code_buffer, 32000);

			send_response = true;
			response_pack << address;
			S64Send(response_pack, (u64)code);
			response_pack << (code_block.m_num_instructions * 4);
			response_pack << host_code_size;

			for (size_t i = 0; i < code_block.m_num_instructions; ++i)
				response_pack << code_buffer.codebuffer[i].inst.hex;

			for (size_t i = 0; i < host_code_size; ++i)
				response_pack << code[i];
		}
		break;
		case CCMD_GETARCH:
		{
			send_response = true;
#if defined(_M_X86)
			response_pack << "x86";
#elif defined(_M_ARM_64)
			response_pack << "aarch64";
#else
			response_pack << "UNK";
#endif
		}
		break;
		case CCMD_SETCOREFEATURE:
		{
			std::string core_part;
			u32 disabled;
			packet >> core_part;
			packet >> disabled;
			std::unordered_map<std::string, bool*> features = {
				{"jit",             &SConfig::GetInstance().bJITOff},
				{"loadstore",       &SConfig::GetInstance().bJITLoadStoreOff},
				{"loadstorefp",     &SConfig::GetInstance().bJITLoadStoreFloatingOff},
				{"loadstorepaired", &SConfig::GetInstance().bJITLoadStorePairedOff},
				{"float",           &SConfig::GetInstance().bJITFloatingPointOff},
				{"integer",         &SConfig::GetInstance().bJITIntegerOff},
				{"paired",          &SConfig::GetInstance().bJITPairedOff},
				{"system",          &SConfig::GetInstance().bJITSystemRegistersOff},
			};
			std::transform(core_part.begin(), core_part.end(), core_part.begin(), ::tolower);
			*features[core_part] = !!disabled;
		}
		break;
		case CCMD_GETCOREFEATURES:
			send_response = true;
			response_pack << (u32)SConfig::GetInstance().bJITOff;
			response_pack << (u32)SConfig::GetInstance().bJITLoadStoreOff;
			response_pack << (u32)SConfig::GetInstance().bJITLoadStoreFloatingOff;
			response_pack << (u32)SConfig::GetInstance().bJITLoadStorePairedOff;
			response_pack << (u32)SConfig::GetInstance().bJITFloatingPointOff;
			response_pack << (u32)SConfig::GetInstance().bJITIntegerOff;
			response_pack << (u32)SConfig::GetInstance().bJITPairedOff;
			response_pack << (u32)SConfig::GetInstance().bJITSystemRegistersOff;
		break;
		}

		if (send_response)
		{
			ENetPacket* epac = enet_packet_create(response_pack.getData(), response_pack.getDataSize(), ENET_PACKET_FLAG_RELIABLE);
			enet_peer_send(peer, 0, epac);
		}
	}

	void ServerThread()
	{
		int event_status;
		ENetEvent event;
		while (m_running)
		{
			event_status = enet_host_service(m_server.get(), &event, 50000);
			if (event_status > 0)
			{
				switch (event.type)
				{
					case ENET_EVENT_TYPE_CONNECT:
						OSD::AddMessage("Debugger connected!");
					break;
					case ENET_EVENT_TYPE_RECEIVE:
					{
						sf::Packet rpac;
						rpac.append(event.packet->data, event.packet->dataLength);
						HandlePacket(rpac, event.peer);
						enet_packet_destroy(event.packet);
					}
					break;
					case ENET_EVENT_TYPE_DISCONNECT:
						OSD::AddMessage("Debugger disconnected!");
						WARN_LOG(COMMON, "(SERVER) We got a disconnect from %x\n", event.peer->address.host);
					break;
					case ENET_EVENT_TYPE_NONE:
					break;
				}
			}
		}
	}
	DebugHandlerServer()
	{
		if (enet_initialize() != 0)
			goto cleanup;
		ENetAddress serverAddr;
		serverAddr.host = ENET_HOST_ANY;
		serverAddr.port = DEBUG_PORT;
		m_server.reset(enet_host_create(&serverAddr, 10, 3, 0, 0));
		if (m_server != nullptr)
			m_server->intercept = ENetUtil::InterceptCallback;
		else
			goto cleanup;

		m_running = true;
		m_thread = std::thread(&DebugHandlerServer::ServerThread, this);
		return;
cleanup:
		m_inited = false;
		m_running = false;
	}
	~DebugHandlerServer()
	{
		m_inited = false;
		m_running = false;
		m_thread.join();
	}
	bool IsAvailable()
	{
		return m_inited && m_running;
	}
};

// Clients
std::unique_ptr<DebugHandler> InitLocal()
{
	return std::unique_ptr<DebugHandler>(new LocalDebugHandler());
}

std::unique_ptr<DebugHandler> InitClient(const std::string& ip)
{
	return std::unique_ptr<DebugHandler>(new ClientDebugHandler(ip));
}

// Server
std::unique_ptr<DebugHandlerServerBase> InitServer()
{
	return std::unique_ptr<DebugHandlerServerBase>(new DebugHandlerServer());
}

}
