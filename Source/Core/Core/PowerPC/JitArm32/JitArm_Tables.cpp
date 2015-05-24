// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitArm_Tables.h"

// Should be moved in to the Jit class
typedef void (JitArm::*_Instruction) (UGeckoInstruction instCode);

static _Instruction dynaOpTable[64];
static _Instruction dynaOpTable4[1024];
static _Instruction dynaOpTable19[1024];
static _Instruction dynaOpTable31[1024];
static _Instruction dynaOpTable59[32];
static _Instruction dynaOpTable63[1024];

void JitArm::DynaRunTable4(UGeckoInstruction _inst)  {(this->*dynaOpTable4 [_inst.SUBOP10])(_inst);}
void JitArm::DynaRunTable19(UGeckoInstruction _inst) {(this->*dynaOpTable19[_inst.SUBOP10])(_inst);}
void JitArm::DynaRunTable31(UGeckoInstruction _inst) {(this->*dynaOpTable31[_inst.SUBOP10])(_inst);}
void JitArm::DynaRunTable59(UGeckoInstruction _inst) {(this->*dynaOpTable59[_inst.SUBOP5 ])(_inst);}
void JitArm::DynaRunTable63(UGeckoInstruction _inst) {(this->*dynaOpTable63[_inst.SUBOP10])(_inst);}

struct GekkoOPTemplate
{
	int opcode;
	_Instruction Inst;
	//GekkoOPInfo opinfo; // Doesn't need opinfo, Interpreter fills it out
};

static GekkoOPTemplate primarytable[] =
{
	{4,  &JitArm::DynaRunTable4},         // RunTable4
	{19, &JitArm::DynaRunTable19},        // RunTable19
	{31, &JitArm::DynaRunTable31},        // RunTable31
	{59, &JitArm::DynaRunTable59},        // RunTable59
	{63, &JitArm::DynaRunTable63},        // RunTable63

	{16, &JitArm::bcx},                   // bcx
	{18, &JitArm::bx},                    // bx

	{3,  &JitArm::twx},                   // twi
	{17, &JitArm::sc},                    // sc

	{7,  &JitArm::arith},                 // mulli
	{8,  &JitArm::subfic},                // subfic
	{10, &JitArm::cmpli},                 // cmpli
	{11, &JitArm::cmpi},                  // cmpi
	{12, &JitArm::arith},                 // addic
	{13, &JitArm::arith},                 // addic_rc
	{14, &JitArm::arith},                 // addi
	{15, &JitArm::arith},                 // addis

	{20, &JitArm::rlwimix},               // rlwimix
	{21, &JitArm::rlwinmx},               // rlwinmx
	{23, &JitArm::rlwnmx},                // rlwnmx

	{24, &JitArm::arith},                 // ori
	{25, &JitArm::arith},                 // oris
	{26, &JitArm::arith},                 // xori
	{27, &JitArm::arith},                 // xoris
	{28, &JitArm::arith},                 // andi_rc
	{29, &JitArm::arith},                 // andis_rc

	{32, &JitArm::lXX},                   // lwz
	{33, &JitArm::lXX},                   // lwzu
	{34, &JitArm::lXX},                   // lbz
	{35, &JitArm::lXX},                   // lbzu
	{40, &JitArm::lXX},                   // lhz
	{41, &JitArm::lXX},                   // lhzu
	{42, &JitArm::lXX},                   // lha
	{43, &JitArm::lXX},                   // lhau

	{44, &JitArm::stX},                   // sth
	{45, &JitArm::stX},                   // sthu
	{36, &JitArm::stX},                   // stw
	{37, &JitArm::stX},                   // stwu
	{38, &JitArm::stX},                   // stb
	{39, &JitArm::stX},                   // stbu

	{46, &JitArm::lmw},                   // lmw
	{47, &JitArm::stmw},                  // stmw

	{48, &JitArm::lfXX},                  // lfs
	{49, &JitArm::lfXX},                  // lfsu
	{50, &JitArm::lfXX},                  // lfd
	{51, &JitArm::lfXX},                  // lfdu

	{52, &JitArm::stfXX},                 // stfs
	{53, &JitArm::stfXX},                 // stfsu
	{54, &JitArm::stfXX},                 // stfd
	{55, &JitArm::stfXX},                 // stfdu

	{56, &JitArm::psq_l},                 // psq_l
	{57, &JitArm::psq_l},                 // psq_lu
	{60, &JitArm::psq_st},                // psq_st
	{61, &JitArm::psq_st},                // psq_stu

	//missing: 0, 1, 2, 5, 6, 9, 22, 30, 62, 58
};

static GekkoOPTemplate table4[] =
{    //SUBOP10
	{0,    &JitArm::FallBackToInterpreter}, // ps_cmpu0
	{32,   &JitArm::FallBackToInterpreter}, // ps_cmpo0
	{40,   &JitArm::ps_neg},                // ps_neg
	{136,  &JitArm::ps_nabs},               // ps_nabs
	{264,  &JitArm::ps_abs},                // ps_abs
	{64,   &JitArm::FallBackToInterpreter}, // ps_cmpu1
	{72,   &JitArm::ps_mr},                 // ps_mr
	{96,   &JitArm::FallBackToInterpreter}, // ps_cmpo1
	{528,  &JitArm::ps_merge00},            // ps_merge00
	{560,  &JitArm::ps_merge01},            // ps_merge01
	{592,  &JitArm::ps_merge10},            // ps_merge10
	{624,  &JitArm::ps_merge11},            // ps_merge11

	{1014, &JitArm::FallBackToInterpreter}, // dcbz_l
};

static GekkoOPTemplate table4_2[] =
{
	{10, &JitArm::ps_sum0},   // ps_sum0
	{11, &JitArm::ps_sum1},   // ps_sum1
	{12, &JitArm::ps_muls0},  // ps_muls0
	{13, &JitArm::ps_muls1},  // ps_muls1
	{14, &JitArm::ps_madds0}, // ps_madds0
	{15, &JitArm::ps_madds1}, // ps_madds1
	{18, &JitArm::ps_div},    // ps_div
	{20, &JitArm::ps_sub},    // ps_sub
	{21, &JitArm::ps_add},    // ps_add
	{23, &JitArm::ps_sel},    // ps_sel
	{24, &JitArm::ps_res},    // ps_res
	{25, &JitArm::ps_mul},    // ps_mul
	{26, &JitArm::ps_rsqrte}, // ps_rsqrte
	{28, &JitArm::ps_msub},   // ps_msub
	{29, &JitArm::ps_madd},   // ps_madd
	{30, &JitArm::ps_nmsub},  // ps_nmsub
	{31, &JitArm::ps_nmadd},  // ps_nmadd
};


static GekkoOPTemplate table4_3[] =
{
	{6,  &JitArm::psq_lx},  // psq_lx
	{7,  &JitArm::psq_stx}, // psq_stx
	{38, &JitArm::psq_lx},  // psq_lux
	{39, &JitArm::psq_stx}, // psq_stux
};

static GekkoOPTemplate table19[] =
{
	{528, &JitArm::bcctrx},    // bcctrx
	{16,  &JitArm::bclrx},     // bclrx
	{257, &JitArm::FallBackToInterpreter},     // crand
	{129, &JitArm::FallBackToInterpreter},     // crandc
	{289, &JitArm::FallBackToInterpreter},     // creqv
	{225, &JitArm::FallBackToInterpreter},     // crnand
	{33,  &JitArm::FallBackToInterpreter},     // crnor
	{449, &JitArm::FallBackToInterpreter},     // cror
	{417, &JitArm::FallBackToInterpreter},     // crorc
	{193, &JitArm::FallBackToInterpreter},     // crxor

	{150, &JitArm::DoNothing}, // isync
	{0,   &JitArm::mcrf},      // mcrf

	{50,  &JitArm::rfi},       // rfi
	{18,  &JitArm::Break},     // rfid
};


static GekkoOPTemplate table31[] =
{
	{266,  &JitArm::arith},                 // addx
	{778,  &JitArm::arith},                 // addox
	{10,   &JitArm::arith},                 // addcx
	{522,  &JitArm::arith},                 // addcox
	{138,  &JitArm::addex},                 // addex
	{650,  &JitArm::addex},                 // addeox
	{234,  &JitArm::FallBackToInterpreter}, // addmex
	{746,  &JitArm::FallBackToInterpreter}, // addmeox
	{202,  &JitArm::FallBackToInterpreter}, // addzex
	{714,  &JitArm::FallBackToInterpreter}, // addzeox
	{491,  &JitArm::FallBackToInterpreter}, // divwx
	{1003, &JitArm::FallBackToInterpreter}, // divwox
	{459,  &JitArm::FallBackToInterpreter}, // divwux
	{971,  &JitArm::FallBackToInterpreter}, // divwuox
	{75,   &JitArm::FallBackToInterpreter}, // mulhwx
	{11,   &JitArm::mulhwux},               // mulhwux
	{235,  &JitArm::arith},                 // mullwx
	{747,  &JitArm::arith},                 // mullwox
	{104,  &JitArm::negx},                  // negx
	{616,  &JitArm::negx},                  // negox
	{40,   &JitArm::arith},                 // subfx
	{552,  &JitArm::arith},                 // subfox
	{8,    &JitArm::FallBackToInterpreter}, // subfcx
	{520,  &JitArm::FallBackToInterpreter}, // subfcox
	{136,  &JitArm::FallBackToInterpreter}, // subfex
	{648,  &JitArm::FallBackToInterpreter}, // subfeox
	{232,  &JitArm::FallBackToInterpreter}, // subfmex
	{744,  &JitArm::FallBackToInterpreter}, // subfmeox
	{200,  &JitArm::FallBackToInterpreter}, // subfzex
	{712,  &JitArm::FallBackToInterpreter}, // subfzeox

	{28,  &JitArm::arith},                  // andx
	{60,  &JitArm::arith},                  // andcx
	{444, &JitArm::arith},                  // orx
	{124, &JitArm::arith},                  // norx
	{316, &JitArm::arith},                  // xorx
	{412, &JitArm::arith},                  // orcx
	{476, &JitArm::arith},                  // nandx
	{284, &JitArm::arith},                  // eqvx
	{0,   &JitArm::cmp},                    // cmp
	{32,  &JitArm::cmpl},                   // cmpl
	{26,  &JitArm::cntlzwx},                // cntlzwx
	{922, &JitArm::extshx},                 // extshx
	{954, &JitArm::extsbx},                 // extsbx
	{536, &JitArm::arith},                  // srwx
	{792, &JitArm::arith},                  // srawx
	{824, &JitArm::srawix},                 // srawix
	{24,  &JitArm::arith},                  // slwx

	{54,   &JitArm::dcbst},                 // dcbst
	{86,   &JitArm::FallBackToInterpreter}, // dcbf
	{246,  &JitArm::DoNothing},             // dcbtst
	{278,  &JitArm::DoNothing},             // dcbt
	{470,  &JitArm::FallBackToInterpreter}, // dcbi
	{758,  &JitArm::DoNothing},             // dcba
	{1014, &JitArm::FallBackToInterpreter}, // dcbz

	//load word
	{23,  &JitArm::lXX},                    // lwzx
	{55,  &JitArm::FallBackToInterpreter},  // lwzux

	//load halfword
	{279, &JitArm::lXX},                    // lhzx
	{311, &JitArm::lXX},                    // lhzux

	//load halfword signextend
	{343, &JitArm::lXX},                    // lhax
	{375, &JitArm::lXX},                    // lhaux

	//load byte
	{87,  &JitArm::lXX},                    // lbzx
	{119, &JitArm::lXX},                    // lbzux

	//load byte reverse
	{534, &JitArm::lXX},                    // lwbrx
	{790, &JitArm::lXX},                    // lhbrx

	// Conditional load/store (Wii SMP)
	{150, &JitArm::FallBackToInterpreter},  // stwcxd
	{20,  &JitArm::FallBackToInterpreter},  // lwarx

	//load string (interpret these)
	{533, &JitArm::FallBackToInterpreter},  // lswx
	{597, &JitArm::FallBackToInterpreter},  // lswi

	//store word
	{151, &JitArm::stX},                    // stwx
	{183, &JitArm::stX},                    // stwux

	//store halfword
	{407, &JitArm::stX},                    // sthx
	{439, &JitArm::stX},                    // sthux

	//store byte
	{215, &JitArm::stX},                    // stbx
	{247, &JitArm::stX},                    // stbux

	//store bytereverse
	{662, &JitArm::FallBackToInterpreter},  // stwbrx
	{918, &JitArm::FallBackToInterpreter},  // sthbrx

	{661, &JitArm::FallBackToInterpreter},  // stswx
	{725, &JitArm::FallBackToInterpreter},  // stswi

	// fp load/store
	{535, &JitArm::lfXX},                   // lfsx
	{567, &JitArm::lfXX},                   // lfsux
	{599, &JitArm::lfXX},                   // lfdx
	{631, &JitArm::lfXX},                   // lfdux

	{663, &JitArm::stfXX},                  // stfsx
	{695, &JitArm::stfXX},                  // stfsux
	{727, &JitArm::stfXX},                  // stfdx
	{759, &JitArm::stfXX},                  // stfdux
	{983, &JitArm::FallBackToInterpreter},  // stfiwx

	{19,  &JitArm::FallBackToInterpreter},  // mfcr
	{83,  &JitArm::mfmsr},                  // mfmsr
	{144, &JitArm::FallBackToInterpreter},  // mtcrf
	{146, &JitArm::mtmsr},                  // mtmsr
	{210, &JitArm::mtsr},                   // mtsr
	{242, &JitArm::FallBackToInterpreter},  // mtsrin
	{339, &JitArm::mfspr},                  // mfspr
	{467, &JitArm::mtspr},                  // mtspr
	{371, &JitArm::mftb},                   // mftb
	{512, &JitArm::FallBackToInterpreter},  // mcrxr
	{595, &JitArm::mfsr},                   // mfsr
	{659, &JitArm::FallBackToInterpreter},  // mfsrin

	{4,   &JitArm::twx},                    // tw
	{598, &JitArm::DoNothing},              // sync
	{982, &JitArm::icbi},                   // icbi

	// Unused instructions on GC
	{310, &JitArm::FallBackToInterpreter},  // eciwx
	{438, &JitArm::FallBackToInterpreter},  // ecowx
	{854, &JitArm::DoNothing},              // eieio
	{306, &JitArm::FallBackToInterpreter},  // tlbie
	{370, &JitArm::FallBackToInterpreter},  // tlbia
	{566, &JitArm::DoNothing},              // tlbsync
};

static GekkoOPTemplate table59[] =
{
	{18, &JitArm::FallBackToInterpreter},  // fdivsx
	{20, &JitArm::fsubsx},                 // fsubsx
	{21, &JitArm::faddsx},                 // faddsx
//	{22, &JitArm::FallBackToInterpreter},    // fsqrtsx
	{24, &JitArm::fresx},                  // fresx
	{25, &JitArm::fmulsx},                 // fmulsx
	{28, &JitArm::FallBackToInterpreter},  // fmsubsx
	{29, &JitArm::fmaddsx},                // fmaddsx
	{30, &JitArm::FallBackToInterpreter},  // fnmsubsx
	{31, &JitArm::fnmaddsx},               // fnmaddsx
};

static GekkoOPTemplate table63[] =
{
	{264, &JitArm::fabsx},                 // fabsx
	{32,  &JitArm::FallBackToInterpreter}, // fcmpo
	{0,   &JitArm::FallBackToInterpreter}, // fcmpu
	{14,  &JitArm::fctiwx},                // fctiwx
	{15,  &JitArm::fctiwzx},               // fctiwzx
	{72,  &JitArm::fmrx},                  // fmrx
	{136, &JitArm::fnabsx},                // fnabsx
	{40,  &JitArm::fnegx},                 // fnegx
	{12,  &JitArm::FallBackToInterpreter}, // frspx

	{64,  &JitArm::FallBackToInterpreter}, // mcrfs
	{583, &JitArm::FallBackToInterpreter}, // mffsx
	{70,  &JitArm::FallBackToInterpreter}, // mtfsb0x
	{38,  &JitArm::FallBackToInterpreter}, // mtfsb1x
	{134, &JitArm::FallBackToInterpreter}, // mtfsfix
	{711, &JitArm::FallBackToInterpreter}, // mtfsfx
};

static GekkoOPTemplate table63_2[] =
{
	{18, &JitArm::FallBackToInterpreter}, // fdivx
	{20, &JitArm::fsubx},                 // fsubx
	{21, &JitArm::faddx},                 // faddx
	{22, &JitArm::FallBackToInterpreter}, // fsqrtx
	{23, &JitArm::fselx},                 // fselx
	{25, &JitArm::fmulx},                 // fmulx
	{26, &JitArm::frsqrtex},              // frsqrtex
	{28, &JitArm::FallBackToInterpreter}, // fmsubx
	{29, &JitArm::fmaddx},                // fmaddx
	{30, &JitArm::FallBackToInterpreter}, // fnmsubx
	{31, &JitArm::fnmaddx},               // fnmaddx
};


namespace JitArmTables
{

void CompileInstruction(PPCAnalyst::CodeOp & op)
{
	JitArm *jitarm = (JitArm *)jit;
	(jitarm->*dynaOpTable[op.inst.OPCD])(op.inst);
	GekkoOPInfo *info = op.opinfo;

	if (info)
	{
#ifdef OPLOG
		if (!strcmp(info->opname, OP_TO_LOG)) // "mcrfs"
		{
			rsplocations.push_back(jit.js.compilerPC);
		}
#endif
		info->compileCount++;
		info->lastUse = jit->js.compilerPC;
	}
}

void InitTables()
{
	// once initialized, tables are read-only
	static bool initialized = false;
	if (initialized)
		return;

	//clear
	for (auto& tpl : dynaOpTable)
	{
		tpl = &JitArm::FallBackToInterpreter;
	}

	for (int i = 0; i < 32; i++)
	{
		dynaOpTable59[i] = &JitArm::FallBackToInterpreter;
	}

	for (int i = 0; i < 1024; i++)
	{
		dynaOpTable4 [i] = &JitArm::FallBackToInterpreter;
		dynaOpTable19[i] = &JitArm::FallBackToInterpreter;
		dynaOpTable31[i] = &JitArm::FallBackToInterpreter;
		dynaOpTable63[i] = &JitArm::FallBackToInterpreter;
	}

	for (int i = 0; i < (int)(sizeof(primarytable) / sizeof(GekkoOPTemplate)); i++)
	{
		dynaOpTable[primarytable[i].opcode] = primarytable[i].Inst;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (int j = 0; j < (int)(sizeof(table4_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill+table4_2[j].opcode;
			dynaOpTable4[op] = table4_2[j].Inst;
		}
	}

	for (int i = 0; i < 16; i++)
	{
		int fill = i << 6;
		for (int j = 0; j < (int)(sizeof(table4_3) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill+table4_3[j].opcode;
			dynaOpTable4[op] = table4_3[j].Inst;
		}
	}

	for (int i = 0; i < (int)(sizeof(table4) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table4[i].opcode;
		dynaOpTable4[op] = table4[i].Inst;
	}

	for (int i = 0; i < (int)(sizeof(table31) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table31[i].opcode;
		dynaOpTable31[op] = table31[i].Inst;
	}

	for (int i = 0; i < (int)(sizeof(table19) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table19[i].opcode;
		dynaOpTable19[op] = table19[i].Inst;
	}

	for (int i = 0; i < (int)(sizeof(table59) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table59[i].opcode;
		dynaOpTable59[op] = table59[i].Inst;
	}

	for (int i = 0; i < (int)(sizeof(table63) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table63[i].opcode;
		dynaOpTable63[op] = table63[i].Inst;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (int j = 0; j < (int)(sizeof(table63_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill + table63_2[j].opcode;
			dynaOpTable63[op] = table63_2[j].Inst;
		}
	}

	initialized = true;

}

}  // namespace
