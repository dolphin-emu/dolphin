// Copyright (C) 2003-2009 Dolphin Project.

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

#ifdef _WIN32
#include "stdafx.h"
#endif
#include "Globals.h"
#include "HLE_Helper.h"


u16& R00 = g_dsp.r[0x00];
u16& R01 = g_dsp.r[0x01];
u16& R02 = g_dsp.r[0x02];
u16& R03 = g_dsp.r[0x03];
u16& R04 = g_dsp.r[0x04];
u16& R05 = g_dsp.r[0x05];
u16& R06 = g_dsp.r[0x06];
u16& R07 = g_dsp.r[0x07];
u16& R08 = g_dsp.r[0x08];
u16& R09 = g_dsp.r[0x09];
u16& R0A = g_dsp.r[0x0a];
u16& R0B = g_dsp.r[0x0b];
u16& R0C = g_dsp.r[0x0c];
u16& R0D = g_dsp.r[0x0d];
u16& R0E = g_dsp.r[0x0e];
u16& R0F = g_dsp.r[0x0f];
u16& R10 = g_dsp.r[0x10];
u16& R11 = g_dsp.r[0x11];
u16& R12 = g_dsp.r[0x12];
u16& R13 = g_dsp.r[0x13];
u16& R14 = g_dsp.r[0x14];
u16& R15 = g_dsp.r[0x15];
u16& R16 = g_dsp.r[0x16];
u16& R17 = g_dsp.r[0x17];
u16& R18 = g_dsp.r[0x18];
u16& R19 = g_dsp.r[0x19];
u16& R1A = g_dsp.r[0x1a];
u16& R1B = g_dsp.r[0x1b];
u16& R1C = g_dsp.r[0x1c];
u16& R1D = g_dsp.r[0x1d];
u16& R1E = g_dsp.r[0x1e];
u16& R1F = g_dsp.r[0x1f];


u16& ST0		= g_dsp.r[0x0c];
u16& ST1		= g_dsp.r[0x0d];
u16& ST2		= g_dsp.r[0x0e];
u16& ST3		= g_dsp.r[0x0f];
u16& ACH0	= g_dsp.r[0x10];
u16& ACH1	= g_dsp.r[0x11];
u16& CR		= g_dsp.r[0x12];
u16& SR		= g_dsp.r[0x13];
u16& PROD_l	= g_dsp.r[0x14];
u16& PROD_m1 = g_dsp.r[0x15];
u16& PROD_h	= g_dsp.r[0x16];
u16& PROD_m2 = g_dsp.r[0x17];
u16& AX0_l	= g_dsp.r[0x18];
u16& AX1_l	= g_dsp.r[0x19];
u16& AX0_h	= g_dsp.r[0x1a];
u16& AX1_h	= g_dsp.r[0x1b];
u16& AC0_l	= g_dsp.r[0x1c];
u16& AC1_l	= g_dsp.r[0x1d];
u16& AC0_m	= g_dsp.r[0x1e];
u16& AC1_m	= g_dsp.r[0x1f];

TAccumulator<0> ACC0;
TAccumulator<1> ACC1;
CProd PROD;


u16 HLE_ROM_80E7_81F8()
{
	s8 MultiplyModifier = GetMultiplyModifier();
// l_80E7:
    AX0_h = ReadDMEM(R00);
    R00++;
    ACC0 = 0;
    Update_SR_Register(ACC0);
// l_80E8:
    AX1_l = ReadDMEM(R01);
    R01++;
    ACC1 = 0;
    Update_SR_Register(ACC1);
// l_80E9:
    AC0_m = ReadDMEM(R02);
    R02++;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_80EA:
    AC0_l = ReadDMEM(R02);
    R02++;
    ACC1 = 0;
    Update_SR_Register(ACC1);
// l_80EB:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
// l_80EC:
    AX0_h = ReadDMEM(R00);
    R00++;
// l_80ED:
    AC1_l = ReadDMEM(R02);
    R02++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_80EE:
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_80EF:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_80F0:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_80F1:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_80F2:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_80F3:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_80F4:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_80F5:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_80F6:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_80F7:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_80F8:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_80F9:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_80FA:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_80FB:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_80FC:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_80FD:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_80FE:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_80FF:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8100:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8101:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_8102:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_8103:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8104:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8105:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8106:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8107:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8108:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8109:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_810A:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_810B:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_810C:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_810D:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_810E:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_810F:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8110:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8111:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_8112:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_8113:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8114:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8115:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8116:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8117:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8118:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8119:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_811A:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_811B:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_811C:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_811D:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_811E:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_811F:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8120:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8121:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_8122:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_8123:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8124:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8125:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8126:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8127:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8128:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8129:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_812A:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_812B:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_812C:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_812D:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_812E:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_812F:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8130:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8131:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_8132:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_8133:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8134:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8135:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8136:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8137:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8138:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8139:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_813A:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_813B:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_813C:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_813D:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_813E:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_813F:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8140:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8141:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_8142:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_8143:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8144:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8145:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8146:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8147:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8148:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8149:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_814A:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_814B:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_814C:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_814D:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_814E:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_814F:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8150:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8151:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_8152:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_8153:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8154:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8155:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8156:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8157:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8158:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8159:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_815A:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_815B:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_815C:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_815D:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_815E:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_815F:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8160:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8161:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_8162:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_8163:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8164:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8165:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8166:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8167:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8168:
    WriteDMEM(R03, AC0_l);
    R03++;
// l_8169:
    ACC0 = PROD;
// l_816A:
    AX0_l = AC0_m;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_816B:
    R01++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_816C:
    WriteDMEM(R03, AC1_m);
    R03++;
// l_816D:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 = 0;
    Update_SR_Register(ACC0);
// l_816E:
    R00 = R04;
// l_816F:
    R02 = R05;
// l_8170:
    R03 = R02;
// l_8171:
    AX0_h = ReadDMEM(R00);
    R00++;
    ACC0 = 0;
    Update_SR_Register(ACC0);
// l_8172:
    AX1_l = ReadDMEM(R01);
    R01++;
    ACC1 = 0;
    Update_SR_Register(ACC1);
// l_8173:
    AC0_m = ReadDMEM(R02);
    R02++;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8174:
    AC0_l = ReadDMEM(R02);
    R02++;
    ACC1 = 0;
    Update_SR_Register(ACC1);
// l_8175:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
// l_8176:
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8177:
    AC1_l = ReadDMEM(R02);
    R02++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8178:
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8179:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_817A:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_817B:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_817C:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_817D:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_817E:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_817F:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8180:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8181:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8182:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8183:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_8184:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_8185:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8186:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8187:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8188:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8189:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_818A:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_818B:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_818C:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_818D:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_818E:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_818F:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8190:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8191:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8192:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8193:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_8194:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_8195:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_8196:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_8197:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_8198:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_8199:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_819A:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_819B:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_819C:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_819D:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_819E:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_819F:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81A0:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81A1:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81A2:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81A3:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81A4:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_81A5:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81A6:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81A7:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81A8:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81A9:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81AA:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81AB:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81AC:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_81AD:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81AE:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81AF:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81B0:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81B1:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81B2:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81B3:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81B4:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_81B5:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81B6:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81B7:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81B8:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81B9:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81BA:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81BB:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81BC:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_81BD:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81BE:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81BF:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81C0:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81C1:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81C2:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81C3:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81C4:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_81C5:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81C6:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81C7:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81C8:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81C9:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81CA:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81CB:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81CC:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_81CD:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81CE:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81CF:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81D0:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81D1:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81D2:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81D3:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81D4:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_81D5:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81D6:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81D7:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81D8:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81D9:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81DA:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81DB:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81DC:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_81DD:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81DE:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81DF:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81E0:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81E1:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81E2:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81E3:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81E4:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_81E5:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81E6:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81E7:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81E8:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81E9:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81EA:
    WriteDMEM(R03, AC0_l);
    R03++;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81EB:
    AC0_m = ReadDMEM(R02);
    R02++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81EC:
    AC0_l = ReadDMEM(R02);
    R02++;
// l_81ED:
    WriteDMEM(R03, AC1_m);
    R03++;
    ACC0 <<= 16;
    Update_SR_Register(ACC0);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81EE:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81EF:
    AC1_m = ReadDMEM(R02);
    R02++;
    ACC0 >>= 16;
    Update_SR_Register(ACC0);
// l_81F0:
    AC1_l = ReadDMEM(R02);
    R02++;
// l_81F1:
    WriteDMEM(R03, AC0_m);
    R03++;
    ACC1 <<= 16;
    Update_SR_Register(ACC1);
    AX0_h = ReadDMEM(R00);
    R00++;
// l_81F2:
    WriteDMEM(R03, AC0_l);
    R03++;
// l_81F3:
    ACC0 = PROD;
// l_81F4:
    AX1_h = AC0_m;
    ACC1 += PROD;
    PROD = AX0_h * AX1_l * MultiplyModifier;
    Update_SR_Register(PROD);
// l_81F5:
    R01++;
    ACC1 >>= 16;
    Update_SR_Register(ACC1);
// l_81F6:
    WriteDMEM(R03, AC1_m);
    R03++;
// l_81F7:
    WriteDMEM(R03, AC1_l);
    R03++;
    ACC0 = 0;
    Update_SR_Register(ACC0);
// l_81F8:
//missing: dsp_opc_ret;

 	return 0x81f8;
}
