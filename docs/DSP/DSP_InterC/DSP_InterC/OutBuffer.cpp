// stdafx.cpp : source file that includes just the standard includes
// DSP_InterC.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

#include <stdarg.h>
#include <stdio.h>

namespace OutBuffer
{
	void Init()
	{

	}

	void Add(const char* _fmt, ...)
	{
		static char Msg[2048];
		va_list ap;

		va_start(ap, _fmt);
		vsprintf(Msg, _fmt, ap);
		va_end(ap);

		printf("%s\n", Msg);
	}

	void AddCode(const char* _fmt, ...)
	{
		static char Msg[2048];
		va_list ap;

		va_start(ap, _fmt);
		vsprintf(Msg, _fmt, ap);
		va_end(ap);

		printf("    %s;\n", Msg);
	}

	// predefined labels
	typedef struct pdlabel_t
	{
		uint16 addr;
		const char* name;
		const char* description;
	} pdlabels_t;

	pdlabel_t regnames[] =
	{
		{0x00, "R00",       "Register 00",},
		{0x01, "R01",       "Register 01",},
		{0x02, "R02",       "Register 02",},
		{0x03, "R03",       "Register 03",},
		{0x04, "R04",       "Register 04",},
		{0x05, "R05",       "Register 05",},
		{0x06, "R06",       "Register 06",},
		{0x07, "R07",       "Register 07",},
		{0x08, "R08",       "Register 08",},
		{0x09, "R09",       "Register 09",},
		{0x0a, "R10",       "Register 10",},
		{0x0b, "R11",       "Register 11",},
		{0x0c, "ST0",       "Call stack",},
		{0x0d, "ST1",       "Data stack",},
		{0x0e, "ST2",       "Loop address stack",},
		{0x0f, "ST3",       "Loop counter",},
		{0x00, "ACH0",      "Accumulator High 0",},
		{0x11, "ACH1",      "Accumulator High 1",},
		{0x12, "CR",        "Config Register",},
		{0x13, "SR",        "Special Register",},
		{0x14, "PROD_l",    "PROD L",},
		{0x15, "PROD_m1",   "PROD M1",},
		{0x16, "PROD_h",    "PROD H",},
		{0x17, "PROD_m2",   "PROD M2",},
		{0x18, "AX0_l",		"Additional Accumulators Low 0",},
		{0x19, "AX1_l", "Additional Accumulators Low 1",},
		{0x1a, "AX0_h", "Additional Accumulators High 0",},
		{0x1b, "AX1_h", "Additional Accumulators High 1",},
		{0x1c, "AC0_l", "Register 28",},
		{0x1d, "AC1_l", "Register 29",},
		{0x1e, "AC0_m", "Register 00",},
		{0x1f, "AC1_m", "Register 00",},

		// additional to resolve special names
		{0x20, "ACC0",      "Accumulators 0",},
		{0x21, "ACC1",      "Accumulators 1",},
		{0x22, "AX0",       "Additional Accumulators 0",},
		{0x23, "AX1",       "Additional Accumulators 1",},
	};

	const pdlabel_t pdlabels[] =
	{
		{0xffa0, "COEF_A1_0", "COEF_A1_0",},
		{0xffa1, "COEF_A2_0", "COEF_A2_0",},
		{0xffa2, "COEF_A1_1", "COEF_A1_1",},
		{0xffa3, "COEF_A2_1", "COEF_A2_1",},
		{0xffa4, "COEF_A1_2", "COEF_A1_2",},
		{0xffa5, "COEF_A2_2", "COEF_A2_2",},
		{0xffa6, "COEF_A1_3", "COEF_A1_3",},
		{0xffa7, "COEF_A2_3", "COEF_A2_3",},
		{0xffa8, "COEF_A1_4", "COEF_A1_4",},
		{0xffa9, "COEF_A2_4", "COEF_A2_4",},
		{0xffaa, "COEF_A1_5", "COEF_A1_5",},
		{0xffab, "COEF_A2_5", "COEF_A2_5",},
		{0xffac, "COEF_A1_6", "COEF_A1_6",},
		{0xffad, "COEF_A2_6", "COEF_A2_6",},
		{0xffae, "COEF_A1_7", "COEF_A1_7",},
		{0xffaf, "COEF_A2_7", "COEF_A2_7",},
		{0xffc9, "DSCR", "DSP DMA Control Reg",},
		{0xffcb, "DSBL", "DSP DMA Block Length",},
		{0xffcd, "DSPA", "DSP DMA DMEM Address",},
		{0xffce, "DSMAH", "DSP DMA Mem Address H",},
		{0xffcf, "DSMAL", "DSP DMA Mem Address L",},
		{0xffd1, "SampleFormat", "SampleFormat",},

		{0xffd3, "Unk Zelda", "Unk Zelda writes to it",},

		{0xffd4, "ACSAH", "Accelerator start address H",},
		{0xffd5, "ACSAL", "Accelerator start address L",},
		{0xffd6, "ACEAH", "Accelerator end address H",},
		{0xffd7, "ACEAL", "Accelerator end address L",},
		{0xffd8, "ACCAH", "Accelerator current address H",},
		{0xffd9, "ACCAL", "Accelerator current address L",},
		{0xffda, "pred_scale", "pred_scale",},
		{0xffdb, "yn1", "yn1",},
		{0xffdc, "yn2", "yn2",},
		{0xffdd, "ARAM", "Direct Read from ARAM (uses ADPCM)",},
		{0xffde, "GAIN", "Gain",},
		{0xffef, "AMDM", "ARAM DMA Request Mask",},
		{0xfffb, "DIRQ", "DSP IRQ Request",},
		{0xfffc, "DMBH", "DSP Mailbox H",},
		{0xfffd, "DMBL", "DSP Mailbox L",},
		{0xfffe, "CMBH", "CPU Mailbox H",},
		{0xffff, "CMBL", "CPU Mailbox L",},
	};

	const char* GetRegName(uint16 reg)
	{
		return regnames[reg].name;
	}

	const char* GetMemName(uint16 addr)
	{
		static char Buffer[1024];
		for (int i=0; i<sizeof(pdlabels); i++)
		{
			if (pdlabels[i].addr == addr)
				return pdlabels[i].name;
		}

		sprintf(Buffer, "0x%4x", addr);
		return Buffer;
	}
}

