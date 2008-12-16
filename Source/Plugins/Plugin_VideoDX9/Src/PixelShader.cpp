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

#include "PixelShader.h"
#include "BPStructs.h"
#include "XFStructs.h"

/*
   old tev->pixelshader notes

   color for this stage (alpha, color) is given by bpmem.tevorders[0].colorchan0
   konstant for this stage (alpha, color) is given by bpmem.tevksel   
   inputs are given by bpmem.combiners[0].colorC.a/b/c/d     << could be current chan color
   according to GXTevColorArg table above
   output is given by .outreg
   tevtemp is set according to swapmodetables and 
*/

const float epsilon = 1.0f/255.0f;

const char *tevKSelTableC[] =
{
	"1,1,1",            //KCSEL_1   = 0x00
	"0.875,0.875,0.875",//KCSEL_7_8 = 0x01
	"0.75,0.75,0.75",	//KCSEL_3_4 = 0x02
	"0.625,0.625,0.625",//KCSEL_5_8 = 0x03
	"0.5,0.5,0.5",      //KCSEL_1_2 = 0x04
	"0.375,0.375,0.375",//KCSEL_3_8 = 0x05
	"0.25,0.25,0.25",   //KCSEL_1_4 = 0x06
	"0.125,0.125,0.125",//KCSEL_1_8 = 0x07
	"ERROR", //0x08
	"ERROR", //0x09
	"ERROR", //0x0a
	"ERROR", //0x0b
	"k0.rgb",//KCSEL_K0 = 0x0C
	"k1.rgb",//KCSEL_K1 = 0x0D
	"k2.rgb",//KCSEL_K2 = 0x0E
	"k3.rgb",//KCSEL_K3 = 0x0F
	"k0.rrr",//KCSEL_K0_R = 0x10
	"k1.rrr",//KCSEL_K1_R = 0x11
	"k2.rrr",//KCSEL_K2_R = 0x12
	"k3.rrr",//KCSEL_K3_R = 0x13
	"k0.ggg",//KCSEL_K0_G = 0x14 
	"k1.ggg",//KCSEL_K1_G = 0x15
	"k2.ggg",//KCSEL_K2_G = 0x16
	"k3.ggg",//KCSEL_K3_G = 0x17
	"k0.bbb",//KCSEL_K0_B = 0x18
	"k1.bbb",//KCSEL_K1_B = 0x19
	"k2.bbb",//KCSEL_K2_B = 0x1A
	"k3.bbb",//KCSEL_K3_B = 0x1B
	"k0.aaa",//KCSEL_K0_A = 0x1C
	"k1.aaa",//KCSEL_K1_A = 0x1D
	"k2.aaa",//KCSEL_K2_A = 0x1E
	"k3.aaa",//KCSEL_K3_A = 0x1F
};
const char *tevKSelTableA[] =
{
	"1",    //KASEL_1   = 0x00
	"0.875",//KASEL_7_8 = 0x01
	"0.75",	//KASEL_3_4 = 0x02
	"0.625",//KASEL_5_8 = 0x03
	"0.5",  //KASEL_1_2 = 0x04
	"0.375",//KASEL_3_8 = 0x05
	"0.25", //KASEL_1_4 = 0x06
	"0.125",//KASEL_1_8 = 0x07
	"ERROR",//0x08
	"ERROR",//0x09
	"ERROR",//0x0a
	"ERROR",//0x0b
	"ERROR",//0x0c
	"ERROR",//0x0d
	"ERROR",//0x0e
	"ERROR",//0x0f
	"k0.r", //KASEL_K0_R = 0x10
	"k1.r", //KASEL_K1_R = 0x11
	"k2.r", //KASEL_K2_R = 0x12
	"k3.r", //KASEL_K3_R = 0x13
	"k0.g", //KASEL_K0_G = 0x14
	"k1.g", //KASEL_K1_G = 0x15
	"k2.g", //KASEL_K2_G = 0x16
	"k3.g", //KASEL_K3_G = 0x17
	"k0.b", //KASEL_K0_B = 0x18
	"k1.b", //KASEL_K1_B = 0x19
	"k2.b", //KASEL_K2_B = 0x1A
	"k3.b", //KASEL_K3_B = 0x1B
	"k0.a", //KASEL_K0_A = 0x1C
	"k1.a", //KASEL_K1_A = 0x1D
	"k2.a", //KASEL_K2_A = 0x1E
	"k3.a", //KASEL_K3_A = 0x1F
};

const char *tevScaleTable[] = 
{
	"1",   //SCALE_1
	"2",   //SCALE_2
	"4",   //SCALE_4
	"0.5", //DIVIDE_2
};

const char *tevBiasTable[] = 
{
	"",      //ZERO,
    "+0.5",  //ADD_HALF,
    "-0.5",  //SUB_HALF,
	"",      //WTF? seen in shadow2
};

const char *tevOpTable[] = 
{
	"+",      //ADD = 0,
    "-",      //SUB = 1,
};

const char *tevCompOpTable[] = 
{
	">",
	"==",
};


#define TEV_COMP_R8    0
#define TEV_COMP_GR16  1
#define TEV_COMP_BGR24 2
#define TEV_COMP_RGB8  3

const char *tevCInputTable[] = 
{
	"prev.rgb",           //CPREV,
    "prev.aaa",           //APREV,
    "c0.rgb",             //C0,
    "c0.aaa",             //A0,
    "c1.rgb",             //C1,
    "c1.aaa",             //A1,
    "c2.rgb",             //C2,
    "c2.aaa",             //A2,
    "textemp.rgb",        //TEXC,
    "textemp.aaa",        //TEXA,
    "rastemp.rgb",        //RASC,
    "rastemp.aaa",        //RASA,
    "float3(1,1,1)",      //ONE,
    "float3(.5,.5,.5)",   //HALF,
    "konsttemp.rgb",      //KONST,
    "float3(0,0,0)",      //ZERO
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
};
const char *tevCInputTable2[] = 
{
	"prev",                 //CPREV,
	"(prev.aaa)",           //APREV,
	"c0",                   //C0,
	"(c0.aaa)",             //A0,
	"c1",                   //C1,
	"(c1.aaa)",             //A1,
	"c2",                   //C2,
	"(c2.aaa)",             //A2,
	"textemp",              //TEXC,
	"(textemp.aaa)",        //TEXA,
	"rastemp",              //RASC,
	"(rastemp.aaa)",        //RASA,
	"float3(1,1,1)",        //ONE,
	"float3(.5,.5,.5)",     //HALF,
	"konsttemp",            //KONST,
	"float3(0,0,0)",        //ZERO
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
};

const char *tevAInputTable[] = 
{
    "prev.a",           //APREV,
    "c0.a",             //A0,
    "c1.a",             //A1,
    "c2.a",             //A2,
    "textemp.a",        //TEXA,
    "rastemp.a",        //RASA,
    "konsttemp.a",      //KONST,  (hw1 had quarter)
    "0.0",				//ZERO
	"PADERROR", "PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR", "PADERROR",
};

const char *tevAInputTable1[] = 
{
	"prev.r",           //APREV,
	"c0.r",             //A0,
	"c1.r",             //A1,
	"c2.r",             //A2,
	"textemp.r",        //TEXA,
	"rastemp.r",        //RASA,
	"konsttemp.r",      //KONST,  (hw1 had quarter)
	"0.0",				//ZERO
	"PADERROR", "PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR", "PADERROR",
};	

const char *tevAInputTable2[] = 
{
    "prev",           //APREV,
    "c0",             //A0,
    "c1",             //A1,
    "c2",             //A2,
    "textemp",        //TEXA,
    "rastemp",        //RASA,
    "konsttemp",      //KONST,  (hw1 had quarter)
    "float4(0,0,0,0)",//ZERO
	"PADERROR", "PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
	"PADERROR",	"PADERROR",	"PADERROR", "PADERROR",
};	

const char *tevRasTable[] = 
{
	"colors[0]",//RAS1_CC_0	0x00000000 /* color channel 0 */
	"colors[1]",//RAS1_CC_1	0x00000001 /* color channel 1 */
	"ERROR", //2
	"ERROR", //3
	"ERROR", //4
	"alphabump", //RAS1_CC_B	0x00000005 /* indirect texture bump alpha */ //green cuz unsupported
	"(alphabump*(255.0f/248.0f))", //RAS1_CC_BN	0x00000006 /* ind tex bump alpha, normalized 0-255 *///green cuz unsupported
	"float4(0,0,0,0)", //RAS1_CC_Z	0x00000007 /* set color value to zero */
};

const char *tevCOutputTable[]  = { "prev.rgb", "c0.rgb", "c1.rgb", "c2.rgb" };
const char *tevAOutputTable[]  = { "prev.a", "c0.a", "c1.a", "c2.a" };
const char *tevIndAlphaSel[]   = {"", "x", "y", "z"};
const char *tevIndAlphaScale[] = {"", "*32","*16","*8"};
const char *tevIndBiasField[]  = {"", "x", "y", "xy", "z", "xz", "yz", "xyz"}; // indexed by bias
const char *tevIndBiasAdd[]    = {"-128.0f", "1.0f", "1.0f", "1.0f" }; // indexed by fmt
const char *tevIndWrapStart[]  = {"0", "256", "128", "64", "32", "16", "0.001" };
const char *tevIndFmtScale[]   = {"255.0f", "31.0f", "15.0f", "8.0f" };

const char *tevTexFuncs[] = 
{
	"tex2D",
	"tex2Dproj"
};

const char *alphaRef[2] = 
{
	"alphaRef.x",
	"alphaRef.y"
};



char text[65536];
#define WRITE p+=sprintf


void WriteStage(char *&p, int n);
void WriteAlphaTest(char *&p);


char *swapColors = "rgba";
char swapModeTable[4][5];

void BuildSwapModeTable()
{
	for (int i = 0; i < 4; i++)
	{
		swapModeTable[i][0] = swapColors[bpmem.tevksel[i*2].swap1];
		swapModeTable[i][1] = swapColors[bpmem.tevksel[i*2].swap2];
		swapModeTable[i][2] = swapColors[bpmem.tevksel[i*2+1].swap1];
		swapModeTable[i][3] = swapColors[bpmem.tevksel[i*2+1].swap2];
		swapModeTable[i][4] = 0;
	}
}

const char *GeneratePixelShader()
{
	BuildSwapModeTable();
	int numStages = bpmem.genMode.numtevstages + 1;
	int numTexgen = bpmem.genMode.numtexgens;
	int numSamplers = 8;

	char *p = text;
	WRITE(p,"//Pixel Shader for TEV stages\n\
//%i TEV stages, %i texgens, %i IND stages, %i COL channels\n",
		bpmem.genMode.numtevstages,bpmem.genMode.numtexgens,bpmem.genMode.numindstages,bpmem.genMode.numcolchans);

	//write kcolor declarations
	for (int i = 0; i < 4; i++) {
		if(i < 3) {
			WRITE(p,"float4 k%i : register(c%i);\n\
                     float4 color%i : register(c%i);\n",i,PS_CONST_KCOLORS+i, i,PS_CONST_COLORS+i+1);
		} else {
			WRITE(p,"float4 k%i : register(c%i);\n",i,PS_CONST_KCOLORS+i);
		}
	}

	WRITE(p,"float constalpha : register(c%i);\n\
float2 alphaRef : register(c%i);\n\n\
sampler samp[%i] : register(s0);\n\n\
float4 main(in float4 colors[2] : COLOR0",PS_CONST_CONSTALPHA,PS_CONST_ALPHAREF,numSamplers);

	if (numTexgen)
		WRITE(p,", float4 uv[%i] : TEXCOORD0",numTexgen);
	else
		WRITE(p,", float4 uv[1] : TEXCOORD0"); //HACK
	WRITE(p,") : COLOR\n\
{\n\
float4 c0=color0,c1=color1,c2=color2,prev=float4(0.0f,0.0f,0.0f,0.0f),textemp,rastemp,konsttemp;\n\
float3 comp16 = float3(1,255,0), comp24 = float3(1,255,255*255);\n\
\n");

	for (int i = 0; i < numStages; i++)
		WriteStage(p,i); //build the equation for this stage

	WriteAlphaTest(p);
	
	/* see GL shader generator - this is Donko's hack
	if (bpmem.dstalpha.enable)
		WRITE(p,"  return float4(prev.rgb,constalpha.x);\n");
	else
	*/
		WRITE(p,"  return prev;\n");

	WRITE(p,"}\n\0");
	
	return text;
}

void WriteStage(char *&p, int n)
{
	const char *rasswap = swapModeTable[bpmem.combiners[n].alphaC.rswap];
	const char *texswap = swapModeTable[bpmem.combiners[n].alphaC.tswap];

	int texfun = xfregs.texcoords[n].texmtxinfo.projection;

	WRITE(p,"rastemp=%s.%s;\n",tevRasTable[bpmem.tevorders[n/2].getColorChan(n&1)],rasswap);
	if (bpmem.tevorders[n/2].getEnable(n&1))
		WRITE(p,"textemp=%s(samp[%i],uv[%i]).%s;\n",
			tevTexFuncs[texfun],
			bpmem.tevorders[n/2].getTexMap(n&1),
			bpmem.tevorders[n/2].getTexCoord(n&1),texswap);
	else
		WRITE(p,"textemp=float4(1,1,1,1);\n");

	int kc = bpmem.tevksel[n/2].getKC(n&1);
	int ka = bpmem.tevksel[n/2].getKA(n&1);

	WRITE(p,"konsttemp=float4(%s,%s);\n",tevKSelTableC[kc],tevKSelTableA[ka]);

	TevStageCombiner::ColorCombiner &cc = bpmem.combiners[n].colorC;
	TevStageCombiner::AlphaCombiner &ac = bpmem.combiners[n].alphaC;

	WRITE(p,"float4(%s,%s)=", tevCOutputTable[cc.dest], tevAOutputTable[ac.dest]); 

	//////////////////////////////////////////////////////////////////////////
	//start of color
	//////////////////////////////////////////////////////////////////////////
	WRITE(p,"float4(\n"); 

	if (cc.bias != TB_COMPARE)
	{
		//normal color combiner goes here
		WRITE(p,"   %s*(%s%s",tevScaleTable[cc.shift],tevCInputTable[cc.d],tevOpTable[cc.op]);
		WRITE(p,"(lerp(%s,%s,%s)%s)),\n",
			tevCInputTable[cc.a],tevCInputTable[cc.b],
			tevCInputTable[cc.c],tevBiasTable[cc.bias]);
	}
	else
	{
		//compare color combiner goes here
		switch(cc.shift)  // yep comparemode stored here :P
		{
		case TEV_COMP_R8:
			if (cc.op == 0)  //equality check needs tolerance, fp in gpu has drawbacks :(
				WRITE(p,"   %s + ((%s.r > %s.r) ? %s : float3(0,0,0)),\n",
				tevCInputTable[cc.d],tevCInputTable2[cc.a],
				tevCInputTable2[cc.b],tevCInputTable[cc.c]);
			else
				WRITE(p,"   %s + (abs(%s.r - %s.r)<%f ? %s : float3(0,0,0)),\n",
				tevCInputTable[cc.d],tevCInputTable2[cc.a],
				tevCInputTable2[cc.b],epsilon,tevCInputTable[cc.c]);
			break;
		default:
			WRITE(p,"float3(0,0,0),\n");
			break;
		}
	}
	//end of color
	
	//////////////////////////////////////////////////////////////////////////
	//start of alpha
	//////////////////////////////////////////////////////////////////////////
	if (ac.bias != TB_COMPARE)
	{
		//normal alpha combiner goes here
		WRITE(p,"   %s*(%s%s",tevScaleTable[ac.shift],tevAInputTable[ac.d],tevOpTable[ac.op]);
		WRITE(p,"lerp(%s,%s,%s) %s)\n",
			tevAInputTable[ac.a],tevAInputTable[ac.b],
			tevAInputTable[ac.c],tevBiasTable[ac.bias]);
	}
	else
	{
        int cmp = (ac.shift<<1)|ac.op|8; // comparemode stored here
		const char **inputTable = NULL;
		inputTable = (cmp == TEVCMP_R8_GT || cmp == TEVCMP_R8_EQ) ? tevAInputTable1 : tevAInputTable;
		//compare alpha combiner goes here
        switch(cmp) {
        case TEVCMP_R8_GT:
        case TEVCMP_A8_GT:
			WRITE(p,"   %s + ((%s > %s) ? %s : 0)\n",
				tevAInputTable[ac.d], inputTable[ac.a], inputTable[ac.b], tevAInputTable[ac.c]);
            break;
        case TEVCMP_R8_EQ:
        case TEVCMP_A8_EQ:
            WRITE(p,"   %s + (abs(%s - %s)<%f ? %s : 0)\n",
                tevAInputTable[ac.d], inputTable[ac.a], inputTable[ac.b],epsilon,tevAInputTable[ac.c]);
            break;
        
        case TEVCMP_GR16_GT: // 16 bit compares: 255*g+r (probably used for ztextures, so make sure in ztextures, g is the most significant byte)
        case TEVCMP_BGR24_GT: // 24 bit compares: 255*255*b+255*g+r
            WRITE(p,"   %s + (( dot(%s.rgb-%s.rgb, comp%s) > 0) ? %s : 0)\n",
                tevAInputTable[ac.d],tevAInputTable2[ac.a], tevAInputTable2[ac.b], cmp==TEVCMP_GR16_GT?"16":"24", tevAInputTable[ac.c]);
            break;
        case TEVCMP_GR16_EQ:
        case TEVCMP_BGR24_EQ:
            WRITE(p,"   %s + (abs(dot(%s.rgb - %s.rgb, comp%s))<%f ? %s : 0)\n",
                tevAInputTable[ac.d],tevAInputTable2[ac.a], tevAInputTable2[ac.b],cmp==TEVCMP_GR16_EQ?"16":"24",epsilon,tevAInputTable[ac.c]);
            break;
        default:
            WRITE(p,"0)\n");
            break;
        }
	}
	WRITE(p, ");");
    if (ac.clamp)
        WRITE(p, "%s = clamp(%s, 0.0f, 1.0f);\n", tevAOutputTable[ac.dest], tevAOutputTable[ac.dest]);
	WRITE(p, "\n");
}

void WriteAlphaCompare(char *&p, int num, int comp)
{
	WRITE(p,"  res%i = ",num);
	switch(comp) {
	case ALPHACMP_ALWAYS:  WRITE(p,"0;\n");	break;
	case ALPHACMP_NEVER:   WRITE(p,"1;\n");	break;
	case ALPHACMP_LEQUAL:  WRITE(p,"prev.a - %s.x;\n",alphaRef[num]);	break;
	case ALPHACMP_LESS:    WRITE(p,"prev.a - %s.x + %f;\n",alphaRef[num],epsilon*2);break;
	case ALPHACMP_GEQUAL:  WRITE(p,"%s - prev.a;\n",alphaRef[num]);	break;
	case ALPHACMP_GREATER: WRITE(p,"%s - prev.a + %f;\n",alphaRef[num],epsilon*2);break;
	case ALPHACMP_EQUAL:   WRITE(p,"abs(%s-prev.a)-%f;\n",alphaRef[num],epsilon*2); break;
	case ALPHACMP_NEQUAL:  WRITE(p,"%f-abs(%s-prev.a);\n",epsilon*2,alphaRef[num]); break;
	}
}

void WriteAlphaTest(char *&p)
{
	AlphaOp op = (AlphaOp)bpmem.alphaFunc.logic;
	Compare comp[2] = {(Compare)bpmem.alphaFunc.comp0,(Compare)bpmem.alphaFunc.comp1};

	//first kill all the simple cases
	if (op == ALPHAOP_AND && (comp[0] == COMPARE_ALWAYS && comp[1] == COMPARE_ALWAYS)) return;
	if (op == ALPHAOP_OR  && (comp[0] == COMPARE_ALWAYS || comp[1] == COMPARE_ALWAYS)) return;
	for (int i = 0; i < 2; i++)
	{
		int one = i;
		int other = 1-i;
		switch(op) {
		case ALPHAOP_XOR:
			if (comp[one] == COMPARE_ALWAYS && comp[other] == COMPARE_NEVER) return;
			break;
		case ALPHAOP_XNOR:
			if (comp[one] == COMPARE_ALWAYS && comp[other] == COMPARE_ALWAYS) return;
			if (comp[one] == COMPARE_ALWAYS && comp[other] == COMPARE_NEVER) return;
			break;
		}
	}

	//Ok, didn't get to do the easy way out :P
	// do the general way
	WRITE(p,"float res0, res1;\n");
	WriteAlphaCompare(p, 0, bpmem.alphaFunc.comp0);
	WriteAlphaCompare(p, 1, bpmem.alphaFunc.comp1);
	WRITE(p,"res0 = max(res0, 0);\n");
	WRITE(p,"res1 = max(res1, 0);\n");
	//probably should use lookup textures for some of these :P
	switch(bpmem.alphaFunc.logic) {
	case ALPHAOP_AND: // if both are 0
		WRITE(p,"clip(-(res0+res1)+%f);\n",epsilon);
		break;
	case ALPHAOP_OR: //if either is 0
		WRITE(p,"clip(-res0*res1+%f);\n",epsilon*epsilon);
		break;
	case ALPHAOP_XOR:
		//hmm, this might work:
		WRITE(p,"res0=(res0>0?1:0)-.5;\n");
		WRITE(p,"res1=(res1>0?1:0)-.5;\n");
		WRITE(p,"clip(-res0*res1);\n",epsilon);
		break;
	case ALPHAOP_XNOR:
		WRITE(p,"res0=(res0>0?1:0)-.5;\n");
		WRITE(p,"res1=(res1>0?1:0)-.5;\n");
		WRITE(p,"clip(res0*res1);\n",epsilon);
		break;
	}
}
