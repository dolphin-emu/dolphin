// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "UCode_AX.h"
#include "../../DSP.h"
#include "FileUtil.h"
#include "ConfigManager.h"

#define AX_GC
#include "UCode_AX_Voice.h"

CUCode_AX::CUCode_AX(DSPHLE* dsp_hle, u32 crc)
	: IUCode(dsp_hle, crc)
	, m_work_available(false)
	, m_cmdlist_size(0)
	, m_run_on_thread(SConfig::GetInstance().m_LocalCoreStartupParameter.bDSPThread)
{
	WARN_LOG(DSPHLE, "Instantiating CUCode_AX: crc=%08x", crc);
	m_rMailHandler.PushMail(DSP_INIT);
	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);

	LoadResamplingCoefficients();

	if (m_run_on_thread)
		m_axthread = std::thread(SpawnAXThread, this);
}

CUCode_AX::~CUCode_AX()
{
	if (m_run_on_thread)
	{
		m_cmdlist_size = (u16)-1;	// Special value to signal end
		NotifyAXThread();
		m_axthread.join();
	}

	m_rMailHandler.Clear();
}

void CUCode_AX::LoadResamplingCoefficients()
{
	m_coeffs_available = false;

	std::string filenames[] = {
		File::GetUserPath(D_GCUSER_IDX) + "dsp_coef.bin",
		File::GetSysDirectory() + "/GC/dsp_coef.bin"
	};

	size_t fidx;
	std::string filename;
	for (fidx = 0; fidx < sizeof (filenames) / sizeof (filenames[0]); ++fidx)
	{
		filename = filenames[fidx];
		if (!File::Exists(filename))
			continue;

		if (File::GetSize(filename) != 0x1000)
			continue;

		break;
	}

	if (fidx >= sizeof (filenames) / sizeof (filenames[0]))
		return;

	WARN_LOG(DSPHLE, "Loading polyphase resampling coeffs from %s", filename.c_str());

	File::IOFile fp(filename, "rb");
	fp.ReadBytes(m_coeffs, 0x1000);

	for (u32 i = 0; i < 0x800; ++i)
		m_coeffs[i] = Common::swap16(m_coeffs[i]);

	m_coeffs_available = true;
}

void CUCode_AX::SpawnAXThread(CUCode_AX* self)
{
	self->AXThread();
}

void CUCode_AX::AXThread()
{
	while (true)
	{
		{
			std::unique_lock<std::mutex> lk(m_cmdlist_mutex);
			while (m_cmdlist_size == 0)
				m_cmdlist_cv.wait(lk);
		}

		if (m_cmdlist_size == (u16)-1)	// End of thread signal
			break;

		m_processing.lock();
		HandleCommandList();
		m_cmdlist_size = 0;
		SignalWorkEnd();
		m_processing.unlock();
	}
}

void CUCode_AX::SignalWorkEnd()
{
	// Signal end of processing
	m_rMailHandler.PushMail(DSP_YIELD);
	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
}

void CUCode_AX::NotifyAXThread()
{
	std::unique_lock<std::mutex> lk(m_cmdlist_mutex);
	m_cmdlist_cv.notify_one();
}

void CUCode_AX::StartWorking()
{
	if (m_run_on_thread)
		NotifyAXThread();
	else
		m_work_available = true;
}

void CUCode_AX::HandleCommandList()
{
	// Temp variables for addresses computation
	u16 addr_hi, addr_lo;
	u16 addr2_hi, addr2_lo;
	u16 size;

	u32 pb_addr = 0;

#if 0
	WARN_LOG(DSPHLE, "Command list:");
	for (u32 i = 0; m_cmdlist[i] != CMD_END; ++i)
		WARN_LOG(DSPHLE, "%04x", m_cmdlist[i]);
	WARN_LOG(DSPHLE, "-------------");
#endif

	u32 curr_idx = 0;
	bool end = false;
	while (!end)
	{
		u16 cmd = m_cmdlist[curr_idx++];

		switch (cmd)
		{
			// Some of these commands are unknown, or unused in this AX HLE.
			// We still need to skip their arguments using "curr_idx += N".

			case CMD_SETUP:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				SetupProcessing(HILO_TO_32(addr));
				break;

			case CMD_DL_AND_VOL_MIX:
			{
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				u16 vol_main = m_cmdlist[curr_idx++];
				u16 vol_auxa = m_cmdlist[curr_idx++];
				u16 vol_auxb = m_cmdlist[curr_idx++];
				DownloadAndMixWithVolume(HILO_TO_32(addr), vol_main, vol_auxa, vol_auxb);
				break;
			}

			case CMD_PB_ADDR:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				pb_addr = HILO_TO_32(addr);
				break;

			case CMD_PROCESS:
				ProcessPBList(pb_addr);
				break;

			case CMD_MIX_AUXA:
			case CMD_MIX_AUXB:
				// These two commands are handled almost the same internally.
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				addr2_hi = m_cmdlist[curr_idx++];
				addr2_lo = m_cmdlist[curr_idx++];
				MixAUXSamples(cmd - CMD_MIX_AUXA, HILO_TO_32(addr), HILO_TO_32(addr2));
				break;

			case CMD_UPLOAD_LRS:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				UploadLRS(HILO_TO_32(addr));
				break;

			case CMD_SET_LR:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				SetMainLR(HILO_TO_32(addr));
				break;

			case CMD_UNK_08: curr_idx += 10; break;	// TODO: check

			case CMD_MIX_AUXB_NOWRITE:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				MixAUXSamples(false, 0, HILO_TO_32(addr));
				break;

			case CMD_COMPRESSOR_TABLE_ADDR: curr_idx += 2; break;
			case CMD_UNK_0B: break; // TODO: check other versions
			case CMD_UNK_0C: break; // TODO: check other versions

			case CMD_MORE:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				size = m_cmdlist[curr_idx++];

				CopyCmdList(HILO_TO_32(addr), size);
				curr_idx = 0;
				break;

			case CMD_OUTPUT:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				addr2_hi = m_cmdlist[curr_idx++];
				addr2_lo = m_cmdlist[curr_idx++];
				OutputSamples(HILO_TO_32(addr2), HILO_TO_32(addr));
				break;

			case CMD_END:
				end = true;
				break;

			case CMD_MIX_AUXB_LR:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				addr2_hi = m_cmdlist[curr_idx++];
				addr2_lo = m_cmdlist[curr_idx++];
				MixAUXBLR(HILO_TO_32(addr), HILO_TO_32(addr2));
				break;

			case CMD_SET_OPPOSITE_LR:
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				SetOppositeLR(HILO_TO_32(addr));
				break;

			case CMD_UNK_12:
			{
				u16 samp_val = m_cmdlist[curr_idx++];
				u16 idx = m_cmdlist[curr_idx++];
				addr_hi = m_cmdlist[curr_idx++];
				addr_lo = m_cmdlist[curr_idx++];
				// TODO
				// suppress warnings:
				(void)samp_val; (void)idx;
				break;
			}

			// Send the contents of MAIN LRS, AUXA LRS and AUXB S to RAM, and
			// mix data to MAIN LR and AUXB LR.
			case CMD_SEND_AUX_AND_MIX:
			{
				// Address for Main + AUXA LRS upload
				u16 main_auxa_up_hi = m_cmdlist[curr_idx++];
				u16 main_auxa_up_lo = m_cmdlist[curr_idx++];

				// Address for AUXB S upload
				u16 auxb_s_up_hi = m_cmdlist[curr_idx++];
				u16 auxb_s_up_lo = m_cmdlist[curr_idx++];

				// Address to read data for Main L
				u16 main_l_dl_hi = m_cmdlist[curr_idx++];
				u16 main_l_dl_lo = m_cmdlist[curr_idx++];

				// Address to read data for Main R
				u16 main_r_dl_hi = m_cmdlist[curr_idx++];
				u16 main_r_dl_lo = m_cmdlist[curr_idx++];

				// Address to read data for AUXB L
				u16 auxb_l_dl_hi = m_cmdlist[curr_idx++];
				u16 auxb_l_dl_lo = m_cmdlist[curr_idx++];

				// Address to read data for AUXB R
				u16 auxb_r_dl_hi = m_cmdlist[curr_idx++];
				u16 auxb_r_dl_lo = m_cmdlist[curr_idx++];

				SendAUXAndMix(HILO_TO_32(main_auxa_up), HILO_TO_32(auxb_s_up),
				              HILO_TO_32(main_l_dl), HILO_TO_32(main_r_dl),
				              HILO_TO_32(auxb_l_dl), HILO_TO_32(auxb_r_dl));
				break;
			}

			default:
				ERROR_LOG(DSPHLE, "Unknown command in AX command list: %04x", cmd);
				end = true;
				break;
		}
	}
}

void CUCode_AX::ApplyUpdatesForMs(int curr_ms, u16* pb, u16* num_updates, u16* updates)
{
	u32 start_idx = 0;
	for (int i = 0; i < curr_ms; ++i)
		start_idx += num_updates[i];

	for (u32 i = start_idx; i < start_idx + num_updates[curr_ms]; ++i)
	{
		u16 update_off = Common::swap16(updates[2 * i]);
		u16 update_val = Common::swap16(updates[2 * i + 1]);

		pb[update_off] = update_val;
	}
}

AXMixControl CUCode_AX::ConvertMixerControl(u32 mixer_control)
{
	u32 ret = 0;

	// TODO: find other UCode versions with different mixer_control values
	if (m_CRC == 0x4e8a8b21)
	{
		ret |= MIX_L | MIX_R;
		if (mixer_control & 0x0001) ret |= MIX_AUXA_L | MIX_AUXA_R;
		if (mixer_control & 0x0002) ret |= MIX_AUXB_L | MIX_AUXB_R;
		if (mixer_control & 0x0004)
		{
			ret |= MIX_S;
			if (ret & MIX_AUXA_L) ret |= MIX_AUXA_S;
			if (ret & MIX_AUXB_L) ret |= MIX_AUXB_S;
		}
		if (mixer_control & 0x0008)
		{
			ret |= MIX_L_RAMP | MIX_R_RAMP;
			if (ret & MIX_AUXA_L) ret |= MIX_AUXA_L_RAMP | MIX_AUXA_R_RAMP;
			if (ret & MIX_AUXB_L) ret |= MIX_AUXB_L_RAMP | MIX_AUXB_R_RAMP;
			if (ret & MIX_AUXA_S) ret |= MIX_AUXA_S_RAMP;
			if (ret & MIX_AUXB_S) ret |= MIX_AUXB_S_RAMP;
		}
	}
	else
	{
		if (mixer_control & 0x0001) ret |= MIX_L;
		if (mixer_control & 0x0002) ret |= MIX_R;
		if (mixer_control & 0x0004) ret |= MIX_S;
		if (mixer_control & 0x0008) ret |= MIX_L_RAMP | MIX_R_RAMP | MIX_S_RAMP;
		if (mixer_control & 0x0010) ret |= MIX_AUXA_L;
		if (mixer_control & 0x0020) ret |= MIX_AUXA_R;
		if (mixer_control & 0x0040) ret |= MIX_AUXA_L_RAMP | MIX_AUXA_R_RAMP;
		if (mixer_control & 0x0080) ret |= MIX_AUXA_S;
		if (mixer_control & 0x0100) ret |= MIX_AUXA_S_RAMP;
		if (mixer_control & 0x0200) ret |= MIX_AUXB_L;
		if (mixer_control & 0x0400) ret |= MIX_AUXB_R;
		if (mixer_control & 0x0800) ret |= MIX_AUXB_L_RAMP | MIX_AUXB_R_RAMP;
		if (mixer_control & 0x1000) ret |= MIX_AUXB_S;
		if (mixer_control & 0x2000) ret |= MIX_AUXB_S_RAMP;

		// TODO: 0x4000 is used for Dolby Pro 2 sound mixing
	}

	return (AXMixControl)ret;
}

void CUCode_AX::SetupProcessing(u32 init_addr)
{
	u16 init_data[0x20];

	for (u32 i = 0; i < 0x20; ++i)
		init_data[i] = HLEMemory_Read_U16(init_addr + 2 * i);

	// List of all buffers we have to initialize
	int* buffers[] = {
		m_samples_left,
		m_samples_right,
		m_samples_surround,
		m_samples_auxA_left,
		m_samples_auxA_right,
		m_samples_auxA_surround,
		m_samples_auxB_left,
		m_samples_auxB_right,
		m_samples_auxB_surround
	};

	u32 init_idx = 0;
	for (u32 i = 0; i < sizeof (buffers) / sizeof (buffers[0]); ++i)
	{
		s32 init_val = (s32)((init_data[init_idx] << 16) | init_data[init_idx + 1]);
		s16 delta = (s16)init_data[init_idx + 2];

		init_idx += 3;

		if (!init_val)
		{
			memset(buffers[i], 0, 5 * 32 * sizeof (int));
		}
		else
		{
			for (u32 j = 0; j < 32 * 5; ++j)
			{
				buffers[i][j] = init_val;
				init_val += delta;
			}
		}
	}
}

void CUCode_AX::DownloadAndMixWithVolume(u32 addr, u16 vol_main, u16 vol_auxa, u16 vol_auxb)
{
	int* buffers_main[3] = { m_samples_left, m_samples_right, m_samples_surround };
	int* buffers_auxa[3] = { m_samples_auxA_left, m_samples_auxA_right, m_samples_auxA_surround };
	int* buffers_auxb[3] = { m_samples_auxB_left, m_samples_auxB_right, m_samples_auxB_surround };
	int** buffers[3] = { buffers_main, buffers_auxa, buffers_auxb };
	u16 volumes[3] = { vol_main, vol_auxa, vol_auxb };

	for (u32 i = 0; i < 3; ++i)
	{
		int* ptr = (int*)HLEMemory_Get_Pointer(addr);
		u16 volume = volumes[i];
		for (u32 j = 0; j < 3; ++j)
		{
			int* buffer = buffers[i][j];
			for (u32 k = 0; k < 5 * 32; ++k)
			{
				s64 sample = (s64)(s32)Common::swap32(*ptr++);
				sample *= volume;
				buffer[k] += (s32)(sample >> 15);
			}
		}
	}
}

void CUCode_AX::ProcessPBList(u32 pb_addr)
{
	// Samples per millisecond. In theory DSP sampling rate can be changed from
	// 32KHz to 48KHz, but AX always process at 32KHz.
	const u32 spms = 32;

	AXPB pb;

	while (pb_addr)
	{
		AXBuffers buffers = {{
			m_samples_left,
			m_samples_right,
			m_samples_surround,
			m_samples_auxA_left,
			m_samples_auxA_right,
			m_samples_auxA_surround,
			m_samples_auxB_left,
			m_samples_auxB_right,
			m_samples_auxB_surround
		}};

		if (!ReadPB(pb_addr, pb))
			break;

		u32 updates_addr = HILO_TO_32(pb.updates.data);
		u16* updates = (u16*)HLEMemory_Get_Pointer(updates_addr);

		for (int curr_ms = 0; curr_ms < 5; ++curr_ms)
		{
			ApplyUpdatesForMs(curr_ms, (u16*)&pb, pb.updates.num_updates, updates);

			ProcessVoice(pb, buffers, spms, ConvertMixerControl(pb.mixer_control),
			             m_coeffs_available ? m_coeffs : NULL);

			// Forward the buffers
			for (u32 i = 0; i < sizeof (buffers.ptrs) / sizeof (buffers.ptrs[0]); ++i)
				buffers.ptrs[i] += spms;
		}

		WritePB(pb_addr, pb);
		pb_addr = HILO_TO_32(pb.next_pb);
	}
}

void CUCode_AX::MixAUXSamples(int aux_id, u32 write_addr, u32 read_addr)
{
	int* buffers[3] = { 0 };

	switch (aux_id)
	{
	case 0:
		buffers[0] = m_samples_auxA_left;
		buffers[1] = m_samples_auxA_right;
		buffers[2] = m_samples_auxA_surround;
		break;

	case 1:
		buffers[0] = m_samples_auxB_left;
		buffers[1] = m_samples_auxB_right;
		buffers[2] = m_samples_auxB_surround;
		break;
	}

	// First, we need to send the contents of our AUX buffers to the CPU.
	if (write_addr)
	{
		int* ptr = (int*)HLEMemory_Get_Pointer(write_addr);
		for (u32 i = 0; i < 3; ++i)
			for (u32 j = 0; j < 5 * 32; ++j)
				*ptr++ = Common::swap32(buffers[i][j]);
	}

	// Then, we read the new temp from the CPU and add to our current
	// temp.
	int* ptr = (int*)HLEMemory_Get_Pointer(read_addr);
	for (u32 i = 0; i < 5 * 32; ++i)
		m_samples_left[i] += (int)Common::swap32(*ptr++);
	for (u32 i = 0; i < 5 * 32; ++i)
		m_samples_right[i] += (int)Common::swap32(*ptr++);
	for (u32 i = 0; i < 5 * 32; ++i)
		m_samples_surround[i] += (int)Common::swap32(*ptr++);
}

void CUCode_AX::UploadLRS(u32 dst_addr)
{
	int buffers[3][5 * 32];

	for (u32 i = 0; i < 5 * 32; ++i)
	{
		buffers[0][i] = Common::swap32(m_samples_left[i]);
		buffers[1][i] = Common::swap32(m_samples_right[i]);
		buffers[2][i] = Common::swap32(m_samples_surround[i]);
	}
	memcpy(HLEMemory_Get_Pointer(dst_addr), buffers, sizeof (buffers));
}

void CUCode_AX::SetMainLR(u32 src_addr)
{
	int* ptr = (int*)HLEMemory_Get_Pointer(src_addr);
	for (u32 i = 0; i < 5 * 32; ++i)
	{
		int samp = (int)Common::swap32(*ptr++);
		m_samples_left[i] = samp;
		m_samples_right[i] = samp;
		m_samples_surround[i] = 0;
	}
}

void CUCode_AX::OutputSamples(u32 lr_addr, u32 surround_addr)
{
	int surround_buffer[5 * 32];

	for (u32 i = 0; i < 5 * 32; ++i)
		surround_buffer[i] = Common::swap32(m_samples_surround[i]);
	memcpy(HLEMemory_Get_Pointer(surround_addr), surround_buffer, sizeof (surround_buffer));

	// 32 samples per ms, 5 ms, 2 channels
	short buffer[5 * 32 * 2];

	// Output samples clamped to 16 bits and interlaced RLRLRLRLRL...
	for (u32 i = 0; i < 5 * 32; ++i)
	{
		int left  = m_samples_left[i];
		int right = m_samples_right[i];

		if (left < -32767)  left = -32767;
		if (left > 32767)   left = 32767;
		if (right < -32767) right = -32767;
		if (right >  32767) right = 32767;

		buffer[2 * i] = Common::swap16(right);
		buffer[2 * i + 1] = Common::swap16(left);
	}

	memcpy(HLEMemory_Get_Pointer(lr_addr), buffer, sizeof (buffer));
}

void CUCode_AX::MixAUXBLR(u32 ul_addr, u32 dl_addr)
{
	// Upload AUXB L/R
	int* ptr = (int*)HLEMemory_Get_Pointer(ul_addr);
	for (u32 i = 0; i < 5 * 32; ++i)
		*ptr++ = Common::swap32(m_samples_auxB_left[i]);
	for (u32 i = 0; i < 5 * 32; ++i)
		*ptr++ = Common::swap32(m_samples_auxB_right[i]);

	// Mix AUXB L/R to MAIN L/R, and replace AUXB L/R
	ptr = (int*)HLEMemory_Get_Pointer(dl_addr);
	for (u32 i = 0; i < 5 * 32; ++i)
	{
		int samp = Common::swap32(*ptr++);
		m_samples_auxB_left[i] = samp;
		m_samples_left[i] += samp;
	}
	for (u32 i = 0; i < 5 * 32; ++i)
	{
		int samp = Common::swap32(*ptr++);
		m_samples_auxB_right[i] = samp;
		m_samples_right[i] += samp;
	}
}

void CUCode_AX::SetOppositeLR(u32 src_addr)
{
	int* ptr = (int*)HLEMemory_Get_Pointer(src_addr);
	for (u32 i = 0; i < 5 * 32; ++i)
	{
		int inp = Common::swap32(*ptr++);
		m_samples_left[i] = -inp;
		m_samples_right[i] = inp;
		m_samples_surround[i] = 0;
	}
}

void CUCode_AX::SendAUXAndMix(u32 main_auxa_up, u32 auxb_s_up, u32 main_l_dl,
                              u32 main_r_dl, u32 auxb_l_dl, u32 auxb_r_dl)
{
	// Buffers to upload first
	int* up_buffers[] = {
		m_samples_auxA_left,
		m_samples_auxA_right,
		m_samples_auxA_surround
	};

	// Upload AUXA LRS
	int* ptr = (int*)HLEMemory_Get_Pointer(main_auxa_up);
	for (u32 i = 0; i < sizeof (up_buffers) / sizeof (up_buffers[0]); ++i)
		for (u32 j = 0; j < 32 * 5; ++j)
			*ptr++ = Common::swap32(up_buffers[i][j]);

	// Upload AUXB S
	ptr = (int*)HLEMemory_Get_Pointer(auxb_s_up);
	for (u32 i = 0; i < 32 * 5; ++i)
		*ptr++ = Common::swap32(m_samples_auxB_surround[i]);

	// Download buffers and addresses
	int* dl_buffers[] = {
		m_samples_left,
		m_samples_right,
		m_samples_auxB_left,
		m_samples_auxB_right
	};
	u32 dl_addrs[] = {
		main_l_dl,
		main_r_dl,
		auxb_l_dl,
		auxb_r_dl
	};

	// Download and mix
	for (u32 i = 0; i < sizeof (dl_buffers) / sizeof (dl_buffers[0]); ++i)
	{
		int* dl_src = (int*)HLEMemory_Get_Pointer(dl_addrs[i]);
		for (u32 j = 0; j < 32 * 5; ++j)
			dl_buffers[i][j] += (int)Common::swap32(*dl_src++);
	}
}

void CUCode_AX::HandleMail(u32 mail)
{
	// Indicates if the next message is a command list address.
	static bool next_is_cmdlist = false;
	static u16 cmdlist_size = 0;

	bool set_next_is_cmdlist = false;

	// Wait for DSP processing to be done before answering any mail. This is
	// safe to do because it matches what the DSP does on real hardware: there
	// is no interrupt when a mail from CPU is received.
	m_processing.lock();

	if (next_is_cmdlist)
	{
		CopyCmdList(mail, cmdlist_size);
		StartWorking();
		NotifyAXThread();
	}
	else if (m_UploadSetupInProgress)
	{
		PrepareBootUCode(mail);
	}
	else if (mail == MAIL_RESUME)
	{
		// Acknowledge the resume request
		m_rMailHandler.PushMail(DSP_RESUME);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
	else if (mail == MAIL_NEW_UCODE)
	{
		soundStream->GetMixer()->SetHLEReady(false);
		m_UploadSetupInProgress = true;
	}
	else if (mail == MAIL_RESET)
	{
		m_DSPHLE->SetUCode(UCODE_ROM);
	}
	else if (mail == MAIL_CONTINUE)
	{
		// We don't have to do anything here - the CPU does not wait for a ACK
		// and sends a cmdlist mail just after.
	}
	else if ((mail & MAIL_CMDLIST_MASK) == MAIL_CMDLIST)
	{
		// A command list address is going to be sent next.
		set_next_is_cmdlist = true;
		cmdlist_size = (u16)(mail & ~MAIL_CMDLIST_MASK);
	}
	else
	{
		ERROR_LOG(DSPHLE, "Unknown mail sent to AX::HandleMail: %08x", mail);
	}

	m_processing.unlock();
	next_is_cmdlist = set_next_is_cmdlist;
}

void CUCode_AX::CopyCmdList(u32 addr, u16 size)
{
	if (size >= (sizeof (m_cmdlist) / sizeof (u16)))
	{
		ERROR_LOG(DSPHLE, "Command list at %08x is too large: size=%d", addr, size);
		return;
	}

	for (u32 i = 0; i < size; ++i, addr += 2)
		m_cmdlist[i] = HLEMemory_Read_U16(addr);
	m_cmdlist_size = size;
}

void CUCode_AX::MixAdd(short* out_buffer, int nsamples)
{
	// Should never be called: we do not set HLE as ready.
	// We accurately send samples to RAM instead of directly to the mixer.
}

void CUCode_AX::Update(int cycles)
{
	// Used for UCode switching.
	if (NeedsResumeMail())
	{
		m_rMailHandler.PushMail(DSP_RESUME);
		DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
	}
	else if (m_work_available)
	{
		HandleCommandList();
		m_cmdlist_size = 0;
		SignalWorkEnd();
	}
}

u32 CUCode_AX::GetUpdateMs()
{
	return 5;
}

void CUCode_AX::DoAXState(PointerWrap& p)
{
	p.Do(m_cmdlist);
	p.Do(m_cmdlist_size);

	p.Do(m_samples_left);
	p.Do(m_samples_right);
	p.Do(m_samples_surround);
	p.Do(m_samples_auxA_left);
	p.Do(m_samples_auxA_right);
	p.Do(m_samples_auxA_surround);
	p.Do(m_samples_auxB_left);
	p.Do(m_samples_auxB_right);
	p.Do(m_samples_auxB_surround);
}

void CUCode_AX::DoState(PointerWrap& p)
{
	std::lock_guard<std::mutex> lk(m_processing);

	DoStateShared(p);
	DoAXState(p);
}
