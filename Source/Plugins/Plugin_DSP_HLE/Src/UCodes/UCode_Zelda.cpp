// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// Games that uses this UCode:
// Zelda: The Windwaker, Mario Sunshine, Mario Kart, Twilight Princess

#include "../Globals.h"
#include "UCodes.h"
#include "UCode_Zelda.h"
#include "UCode_Zelda_ADPCM.h"
#include "../MailHandler.h"

#include "../main.h"
#include "Mixer.h"

#include "WaveFile.h"

class CResampler
{
public:
    CResampler(short* samples, int num_stereo_samples, int core_sample_rate)
        : m_mode(1)
        , m_queueSize(0)
    {
        int PV1l=0,PV2l=0,PV3l=0,PV4l=0;
        int acc=0;     

        while (num_stereo_samples) 
        {
            acc += core_sample_rate;
            while (num_stereo_samples && (acc >= 48000)) 
            {
                PV4l=PV3l;
                PV3l=PV2l;
                PV2l=PV1l;
                PV1l=*(samples++); //32bit processing
                num_stereo_samples--;
                acc-=48000;
            }

            // defaults to nearest
            s32 DataL = PV1l;

            if (m_mode == 1) { //linear
                DataL = PV1l + ((PV2l - PV1l)*acc)/48000;
            }
            else if (m_mode == 2) {//cubic
                s32 a0l = PV1l - PV2l - PV4l + PV3l;
                s32 a1l = PV4l - PV3l - a0l;
                s32 a2l = PV1l - PV4l;
                s32 a3l = PV2l;

                s32 t0l = ((a0l    )*acc)/48000;
                s32 t1l = ((t0l+a1l)*acc)/48000;
                s32 t2l = ((t1l+a2l)*acc)/48000;
                s32 t3l = ((t2l+a3l));

                DataL = t3l;
            }

            int l = DataL;
            if (l < -32767) l = -32767;
            if (l > 32767) l = 32767;
            sample_queue.push(l);
            m_queueSize += 1;
        }
    }

    FixedSizeQueue<s16, queue_maxlength> sample_queue;
    int m_queueSize;
    int m_mode;
};

CUCode_Zelda::CUCode_Zelda(CMailHandler& _rMailHandler)
	: IUCode(_rMailHandler)
	, m_numSteps(0)
	, m_step(0)
	, m_readOffset(0)
    , m_NumberOfFramesToRender(0)
    , m_CurrentFrameToRender(0)
    , m_MaxSyncedPB(0)
    , m_MailState(WaitForMail)
{
	DEBUG_LOG(DSPHLE, "UCode_Zelda - add boot mails for handshake");
	m_rMailHandler.PushMail(DSP_INIT);
	g_dspInitialize.pGenerateDSPInterrupt();
	m_rMailHandler.PushMail(0xF3551111); // handshake
	memset(m_Buffer, 0, sizeof(m_Buffer));

    for (int i = 0; i < 0x10; i++)
        m_PBMask[i] = false;

    templbuffer = new int[1024 * 1024];
    temprbuffer = new int[1024 * 1024];
}

CUCode_Zelda::~CUCode_Zelda()
{
	m_rMailHandler.Clear();

    delete [] templbuffer;
    delete [] temprbuffer;
}

void CUCode_Zelda::UpdatePB(ZPB& _rPB, int *templbuffer, int *temprbuffer, u32 _Size)
{
    u16* pTest = (u16*)&_rPB;

    if (pTest[0x00] == 0) 
        return;

    if (pTest[0x01] != 0)
        return;
    
    if (pTest[0x06] != 0x00)
    {
        // probably pTest[0x06] == 0 -> AFC (and variants)
    }
    else
    {
        switch(_rPB.type)  // or Bytes per Sample
        {
        case 0x05:
        case 0x09:
            {
                // initialize "decoder" if the sample is played the first time
                if (pTest[0x04] != 0)
                {
                    // zelda: 
                    // perhaps init or "has played before"
                    pTest[0x32] = 0x00;
					pTest[0x66] = 0x00;     // history1 
                    pTest[0x67] = 0x00;     // history2 

                    // samplerate? length? num of samples? i dunno...
					// Likely length...
                    pTest[0x3a] = pTest[0x8a];
                    pTest[0x3b] = pTest[0x8b];

                    // copy ARAM addr from r to rw area
                    pTest[0x38] = pTest[0x8c];
                    pTest[0x39] = pTest[0x8d];
                }

                if (pTest[0x01] != 0)  // 0747 early out... i dunno if this can happen because we filter it above
                    return;

                u32 ARAMAddr        = (pTest[0x38] << 16) | pTest[0x39];
                u32 NumberOfSamples = (pTest[0x3a] << 16) | pTest[0x3b];

				// round upwards how many samples we need to copy, 0759
                NumberOfSamples = (NumberOfSamples + 0xf) >> 4;   // i think the lower 4 are the fraction
                u32 frac = NumberOfSamples & 0xF;

                u8 inBuffer[9];
                short outbuf[16];
                u32 sampleCount = 0;

				// It must be something like this:

				// The PB contains a small sample buffer of 0x4D decoded samples.
				// If it's empty or "used", decode to it.
				// Then, resample from this buffer to the output as you go. When it needs
				// wrapping, decode more.
				
#define USE_RESAMPLE 1
#if USE_RESAMPLE != 1
                for (int s=0; s<(_Size/16);s++)
                {
                    for (int i=0; i<9; i++)    
                    {
                        inBuffer[i] =  g_dspInitialize.pARAM_Read_U8(ARAMAddr);
                        ARAMAddr++;
                    }
                                        

                    AFCdecodebuffer((char*)inBuffer, outbuf, (short*)&pTest[0x66], (short*)&pTest[0x67]);

                    for (int i=0; i<16; i++)
                    {
                        templbuffer[sampleCount] += outbuf[i];
                        temprbuffer[sampleCount] += outbuf[i];
                        sampleCount++;
                    }

                    NumberOfSamples--;

                    if (NumberOfSamples<=0)
                        break;
                }
#else
                while (NumberOfSamples > 0)
                {
                    for (int i = 0; i < 9; i++)    
                    {
                        inBuffer[i] =  g_dspInitialize.pARAM_Read_U8(ARAMAddr);
                        ARAMAddr++;
                    }

                    AFCdecodebuffer(m_AFCCoefTable, (char*)inBuffer, outbuf, (short*)&pTest[0x66], (short*)&pTest[0x67], 9);
                    CResampler Sampler(outbuf, 16, 48000);

                    while (Sampler.m_queueSize > 0) 
                    {
                        int sample = Sampler.sample_queue.front();
                        Sampler.sample_queue.pop();
                        Sampler.m_queueSize-=1;

                        templbuffer[sampleCount] += sample;
                        temprbuffer[sampleCount] += sample;
                        sampleCount++;

                        if (sampleCount > _Size)
                            break;
                    }

                    if (sampleCount > _Size)
                        break;

                    NumberOfSamples--;
                }
#endif
                if (NumberOfSamples == 0)
                {
                    pTest[0x01] = 1; // we are done ??
                }

                // write back
                NumberOfSamples = (NumberOfSamples << 4);    // missing fraction

                pTest[0x38] = ARAMAddr >> 16;
                pTest[0x39] = ARAMAddr & 0xFFFF;
                pTest[0x3a] = NumberOfSamples >> 16;
                pTest[0x3b] = NumberOfSamples & 0xFFFF;
                

#if 0
                NumberOfSamples = (NumberOfSamples + 0xf) >> 4;
                
                static u8 Buffer[500000];
                for (int i =0; i<NumberOfSamples*9; i++)
                {
                    Buffer[i] = g_dspInitialize.pARAM_Read_U8(ARAMAddr+i);
                }

                // yes, the dumps are really zelda sound ;)
                DumpAFC(Buffer, NumberOfSamples*9, 0x3d00);

                DumpPB(_rPB);

             //   exit(1);
#endif

                // i think  pTest[0x3a] and pTest[0x3b] got an update after you have decoded some samples...
                // just decrement them with the number of samples you have played
                // and incrrease the ARAM Offset in pTest[0x38], pTest[0x39]


                // end of block (Zelda 03b2)
                if (pTest[0x06] == 0)
                {
                    pTest[0x04] = 0;
                }
            }
            break;
        }
    }
}

void CUCode_Zelda::Update(int cycles)
{
	// This is called at arbitrary intervals from the core emu.
	// We don't need it.
}

void CUCode_Zelda::HandleMail(u32 _uMail)
{
    switch (m_MailState)
    {
        case WaitForMail:
            {
                if (_uMail == 0x00000000)
                {
                    m_MailState = ReadingFrameSync;
                }
                else if (_uMail < 0x255)
                {
                    m_numSteps = _uMail;
                    m_MailState = ReadingMessage;
                }
                else
                {
                    PanicAlert("Unknown Mail: 0x%08x", _uMail);
                }
            }
            break;

        case ReadingFrameSync:
            {
                int Slot = (_uMail >> 16) & 0x000F;
                m_PBMask[Slot] = _uMail & 0xFFFF;
                m_MailState = WaitForMail;

                // Zelda UC 05db
                m_MaxSyncedPB = (Slot+1) << 4;
            }
            break;

        case ReadingMessage:
            {
                if (m_step < 0 || m_step >= sizeof(m_Buffer)/4)
                    PanicAlert("m_step out of range");

                ((u32*)m_Buffer)[m_step] = _uMail;
                m_step++;

                if (m_step >= m_numSteps)
                {
                    ExecuteList();
                    m_MailState = WaitForMail;
                    m_step = 0;
                }
            }
            break;

        case ReadingSystemMsg:
            {
                _dbg_assert_msg_(DSPHLE, (_uMail & 0xFFFF0000) == 0xCDD10000, "This is not a system msg", _uMail);
                m_MailState = WaitForMail;
            }
            break;
    }
}

void CUCode_Zelda::MixAdd(short* _pBuffer, int _iSize)
{
    if (m_NumberOfFramesToRender > 0)
    {
        if (m_NumPBs <= m_MaxSyncedPB)  // we just render if all PBs are synced...Zelda does it in steps of 0x10 PBs but hey this is HLE
        {
            if (_iSize > 1024 * 1024)
                _iSize = 1024 * 1024;

            memset(templbuffer, 0, _iSize * sizeof(int));
            memset(temprbuffer, 0, _iSize * sizeof(int));

            CopyPBsFromRAM();

            // render frame...
            for (u32 i = 0; i < m_NumPBs; i++)
            {
                // masking of PBs is done in zelda 0272... skip it for the moment
                /* int Slot = i >> 4;
                int Mask = i & 0x0F;
                if (m_PBMask[Slot] & Mask)) */
                {
                    UpdatePB(m_PBs[i], templbuffer, temprbuffer, _iSize);
                }                
            }
            CopyPBsToRAM();
            m_MaxSyncedPB = 0;

            if (_pBuffer) {
                for (int i = 0; i < _iSize; i++)
                {
                    // Clamp into 16-bit. Maybe we should add a volume compressor here.
                    int left  = templbuffer[i] + _pBuffer[0];
                    int right = temprbuffer[i] + _pBuffer[1];
                    if (left < -32767)  left = -32767;
                    if (left > 32767)   left = 32767;
                    if (right < -32767) right = -32767;
                    if (right >  32767) right = 32767;
                    *_pBuffer++ = left;
                    *_pBuffer++ = right;
                }
            }

        }
        else
        {
            return;
        }

        m_CurrentFrameToRender++;

        // sync, we are ready
        m_rMailHandler.PushMail(DSP_SYNC);
        g_dspInitialize.pGenerateDSPInterrupt();
        m_rMailHandler.PushMail(0xF3550000 | m_CurrentFrameToRender | 0xFF00);

        if (m_CurrentFrameToRender == m_NumberOfFramesToRender)
        {
            /*           
            afaik zelda hasnt registered a handler to FRAME_END... so skip it
            m_rMailHandler.PushMail(DSP_FRAME_END);
            g_dspInitialize.pGenerateDSPInterrupt();
            m_MailState = ReadingSystemMsg;
            */
            m_NumberOfFramesToRender = 0;
            m_CurrentFrameToRender = 0;
        }
    }
}

// zelda debug ..803F6418
void CUCode_Zelda::ExecuteList()
{
	// begin with the list
	m_readOffset = 0;

	u32 CmdMail = Read32();
	u32 Command = (CmdMail >> 24) & 0x7f;
	u32 Sync = CmdMail >> 16;

	DEBUG_LOG(DSPHLE, "==============================================================================");
	DEBUG_LOG(DSPHLE, "Zelda UCode - execute dlist (cmd: 0x%04x : sync: 0x%04x)", Command, Sync);

	switch (Command)
	{
		// DsetupTable ... zelda ww jumps to 0x0095
	    case 0x01:
	    {
            m_NumPBs = (CmdMail & 0xFFFF);
            if (m_NumPBs > 0x40)
            {
                PanicAlert("(m_NumPBs > 0x40) to much PBs");
                m_NumPBs = 0x40;
            }

		    m_PBAddress = Read32();
		    u32 DSPADPCM_FILTER = Read32();
		    u32 DSPRES_FILTER = Read32();
		    m_PBAddress2 = Read32();

			// What is this stuff?
            u16 Buffer[0x280];
            for (int i = 0; i < 0x280; i++) 
            {
                Buffer[i] = Memory_Read_U16(DSPADPCM_FILTER + (i*2));
            }

            u16* pTmp = (u16*)m_AFCCoefTable;
            for (int i = 0; i < 0x20; i++) 
            {
                pTmp[i] = Memory_Read_U16(DSPRES_FILTER + (i*2));
            }

		    DEBUG_LOG(DSPHLE, "DsetupTable");
		    DEBUG_LOG(DSPHLE, "Param Blocks 1:                0x%08x", m_PBAddress);
            DEBUG_LOG(DSPHLE, "DSPADPCM_FILTER (size: 0x500): 0x%08x", DSPADPCM_FILTER);
			DEBUG_LOG(DSPHLE, "DSPRES_FILTER   (size: 0x40):  0x%08x", DSPRES_FILTER);
		    DEBUG_LOG(DSPHLE, "Param Blocks 2:                0x%08x", m_PBAddress2);
	    }
		break;

		// SyncFrame ... zelda ww jumps to 0x0243
        // SyncFrame doesn't send a 0xDCD10004 SYNC at all ... just the 0xDCD10005 for "frame end"
	    case 0x02:
	    {
            m_MixingBufferLeft = Read32();
		    m_MixingBufferRight = Read32();

            m_NumberOfFramesToRender = (CmdMail >> 16) & 0xFF;
            m_MaxSyncedPB = 0;

		    DEBUG_LOG(DSPHLE, "DsyncFrame");
		    DEBUG_LOG(DSPHLE, "Left Mixing Buffer:            0x%08x", m_MixingBufferLeft);
		    DEBUG_LOG(DSPHLE, "Right Mixing Buffer:           0x%08x", m_MixingBufferRight);

			// This is where we should render.
            soundStream->GetMixer()->SetHLEReady(true);
	    }
		return;

/*
		case 0x03: break;   // dunno ... zelda ww jmps to 0x0073
		case 0x04: break;   // dunno ... zelda ww jmps to 0x0580
		case 0x05: break;   // dunno ... zelda ww jmps to 0x0592
		case 0x06: break;   // dunno ... zelda ww jmps to 0x0469
		case 0x07: break;   // dunno ... zelda ww jmps to 0x044d
		case 0x08: break;   // Mixer ... zelda ww jmps to 0x0485
		case 0x09: break;   // dunno ... zelda ww jmps to 0x044d
 */

		    // DsetDolbyDelay ... zelda ww jumps to 0x00b2
	    case 0x0d:
	    {
		    u32 tmp[1];
		    tmp[0] = Read32();
		    DEBUG_LOG(DSPHLE, "DSetDolbyDelay");
		    DEBUG_LOG(DSPHLE, "DOLBY2_DELAY_BUF (size 0x960): 0x%08x", tmp[0]);
	    }
		    break;

	    // Set VARAM
		// Luigi__: in the real Zelda ucode, this opcode is dummy
		// however, in the ucode used by SMG it isn't
	    case 0x0e:
			{
				DEBUG_LOG(DSPHLE, "Set VARAM - ???");
				/*
				 00b0 0080 037d lri         $AR0, #0x037d
				 00b2 0e01      lris        $AC0.M, #0x01
				 00b3 02bf 0065 call        0x0065
				 00b5 00de 037d lr          $AC0.M, @0x037d
				 00b7 0240 7fff andi        $AC0.M, #0x7fff
				 00b9 00fe 037d sr          @0x037d, $AC0.M
				 00bb 029f 0041 jmp         0x0041
				*/
				//
			}
		    break;

		    // default ... zelda ww jumps to 0x0043
	    default:
		    PanicAlert("Zelda UCode - unknown cmd: %x (size %i)", Command, m_numSteps);
		    break;
	}

	// sync, we are ready
	m_rMailHandler.PushMail(DSP_SYNC);
	g_dspInitialize.pGenerateDSPInterrupt();
	m_rMailHandler.PushMail(0xF3550000 | Sync);
}

void CUCode_Zelda::CopyPBsFromRAM()
{
    for (u32 i = 0; i < m_NumPBs; i++)
    {
        u32 addr = m_PBAddress + i * sizeof(ZPB);
        const u16 *src_ptr = (u16 *)g_dspInitialize.pGetMemoryPointer(addr);
        u16 *dst_ptr = (u16 *)&m_PBs[i];

        for (size_t p = 0; p < 0xC0; p++)
        {
            dst_ptr[p] = Common::swap16(src_ptr[p]);
        }
    }
}

void CUCode_Zelda::CopyPBsToRAM()
{
    for (u32 i = 0; i < m_NumPBs; i++)
    {
        u32 addr = m_PBAddress + i * sizeof(ZPB);
        const u16 *src_ptr = (u16 *)&m_PBs[i];
        u16 *dst_ptr = (u16 *)g_dspInitialize.pGetMemoryPointer(addr);
        for (size_t p = 0; p < 0x80; p++)               // we write the first 0x80 shorts back only
        {
            dst_ptr[p] = Common::swap16(src_ptr[p]);
        }
    }
}

// Decoder from in_cube by hcs/destop/etc. Haven't yet found a valid use for it.

// Looking at in_cube, it seems to be 9 bytes of input = 16 samples of output.
// Different from AX ADPCM which is 8 bytes of input = 14 samples of output.
// input = location of encoded source samples
// out = location of destination buffer (16 bits / sample)

// I am sure that there are 5 bytes of input for 16 samples output too... just check the UCode
// if (type == 5) -> 5 input bytes
// if (type == 9) -> 9 input bytes
//

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// --- Debug Helper 
//
///////////////////////////////////////////////////////////////////////////////////////////////////

void CUCode_Zelda::DumpPB(const ZPB& _rPB)
{
    u16* pTmp = (u16*)&_rPB;
    FILE* pF = fopen("d:\\dump\\PB.txt", "a");
    if (pF)
    {
		if (_rPB.addr_high)
		{
			for (int i = 0; i < 0xc0; i += 4)
			{
				fprintf(pF, "[0x%02x] %04x %04x %04x %04x\n", i, pTmp[i], pTmp[i + 1], pTmp[i + 2], pTmp[i + 3]);
			}
			fprintf(pF, "\n");
			fclose(pF);
		}
    }
}

void write32le(int in, unsigned char * buf) {
    buf[0]=in&0xff;
    buf[1]=(in>>8)&0xff;
    buf[2]=(in>>16)&0xff;
    buf[3]=(in>>24)&0xff;
} 

int CUCode_Zelda::DumpAFC(u8* pIn, const int size, const int srate)
{
    unsigned char inbuf[9];
    short outbuf[16];
    FILE * outfile;
    int sizeleft;
    int outsize,outsizetotal;
    short hist=0,hist2=0;

    unsigned char wavhead[44] = {
        0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00,  0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20,
        0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61,  0x00, 0x00, 0x00, 0x00
    };

    outfile = fopen("d:/supa.wav","wb");
    if (!outfile) return 1;

    outsize = size/9*16*2;
    outsizetotal = outsize+8;
    write32le(outsizetotal,wavhead+4);
    write32le(outsize,wavhead+40);
    write32le(srate,wavhead+24);
    write32le(srate*2,wavhead+28);
    if (fwrite(wavhead,1,44,outfile)!=44) return 1;

    for (sizeleft=size;sizeleft>=9;sizeleft-=9) {
        memcpy(inbuf, pIn, 9);
        pIn += 9;

        AFCdecodebuffer(m_AFCCoefTable, (char*)inbuf,outbuf,&hist,&hist2,9);

        if (fwrite(outbuf,1,16*2,outfile) != 16*2)
            return 1;
    }

    if (fclose(outfile)==EOF) return 1;

    return 0;
} 

void CUCode_Zelda::DoState(PointerWrap &p) {
	p.Do(m_MailState);
	p.Do(m_PBMask);
	p.Do(m_NumPBs);
	p.Do(m_PBAddress);
	p.Do(m_MaxSyncedPB);
	p.Do(m_PBs);
	p.Do(m_readOffset);
	p.Do(m_NumberOfFramesToRender);
	p.Do(m_CurrentFrameToRender);
	p.Do(m_numSteps);
	p.Do(m_step);
	p.Do(m_Buffer);
}
