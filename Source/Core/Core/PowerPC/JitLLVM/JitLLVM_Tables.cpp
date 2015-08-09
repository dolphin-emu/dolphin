// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitLLVM/Jit.h"
#include "Core/PowerPC/JitLLVM/JitLLVM_Tables.h"

// Should be moved in to the Jit class
typedef void (JitLLVM::*_Instruction) (LLVMFunction* func, UGeckoInstruction instCode);

static _Instruction dynaOpTable[64];
static _Instruction dynaOpTable4[1024];
static _Instruction dynaOpTable19[1024];
static _Instruction dynaOpTable31[1024];
static _Instruction dynaOpTable59[32];
static _Instruction dynaOpTable63[1024];
void JitLLVM::DynaRunTable4 (LLVMFunction* func, UGeckoInstruction _inst) {(this->*dynaOpTable4 [_inst.SUBOP10])(func, _inst);}
void JitLLVM::DynaRunTable19(LLVMFunction* func, UGeckoInstruction _inst) {(this->*dynaOpTable19[_inst.SUBOP10])(func, _inst);}
void JitLLVM::DynaRunTable31(LLVMFunction* func, UGeckoInstruction _inst) {(this->*dynaOpTable31[_inst.SUBOP10])(func, _inst);}
void JitLLVM::DynaRunTable59(LLVMFunction* func, UGeckoInstruction _inst) {(this->*dynaOpTable59[_inst.SUBOP5 ])(func, _inst);}
void JitLLVM::DynaRunTable63(LLVMFunction* func, UGeckoInstruction _inst) {(this->*dynaOpTable63[_inst.SUBOP10])(func, _inst);}

struct GekkoOPTemplate
{
	int opcode;
	_Instruction Inst;
};
static GekkoOPTemplate primarytable[] =
{
	{4,  &JitLLVM::DynaRunTable4},         // RunTable4
	{19, &JitLLVM::DynaRunTable19},        // RunTable19
	{31, &JitLLVM::DynaRunTable31},        // RunTable31
	{59, &JitLLVM::DynaRunTable59},        // RunTable59
	{63, &JitLLVM::DynaRunTable63},        // RunTable63

	{16, &JitLLVM::bcx},                   // bcx
	{18, &JitLLVM::bx},                    // bx

	{3,  &JitLLVM::FallBackToInterpreter}, // twi
	{17, &JitLLVM::sc},                    // sc

	{7,  &JitLLVM::mulli},                 // mulli
	{8,  &JitLLVM::subfic},                // subfic
	{10, &JitLLVM::cmpli},                 // cmpli
	{11, &JitLLVM::cmpi},                  // cmpi
	{12, &JitLLVM::addic},                 // addic
	{13, &JitLLVM::addic_rc},              // addic_rc
	{14, &JitLLVM::arith_imm},             // addi
	{15, &JitLLVM::arith_imm},             // addis

	{20, &JitLLVM::rlwXX},                 // rlwimix
	{21, &JitLLVM::rlwXX},                 // rlwinmx
	{23, &JitLLVM::rlwXX},                 // rlwnmx

	{24, &JitLLVM::arith_imm},             // ori
	{25, &JitLLVM::arith_imm},             // oris
	{26, &JitLLVM::arith_imm},             // xori
	{27, &JitLLVM::arith_imm},             // xoris
	{28, &JitLLVM::arith_imm},             // andi_rc
	{29, &JitLLVM::arith_imm},             // andis_rc

	{32, &JitLLVM::lwz},                   // lwz
	{33, &JitLLVM::lwzu},                  // lwzu
	{34, &JitLLVM::lbz},                   // lbz
	{35, &JitLLVM::lbzu},                  // lbzu
	{40, &JitLLVM::lhz},                   // lhz
	{41, &JitLLVM::lhzu},                  // lhzu
	{42, &JitLLVM::lha},                   // lha
	{43, &JitLLVM::lhau},                  // lhau

	{44, &JitLLVM::sth},                   // sth
	{45, &JitLLVM::sthu},                  // sthu
	{36, &JitLLVM::stw},                   // stw
	{37, &JitLLVM::stwu},                  // stwu
	{38, &JitLLVM::stb},                   // stb
	{39, &JitLLVM::stbu},                  // stbu

	{46, &JitLLVM::lmw},                   // lmw
	{47, &JitLLVM::stmw},                  // stmw

	{48, &JitLLVM::lfs},                   // lfs
	{49, &JitLLVM::lfsu},                  // lfsu
	{50, &JitLLVM::lfd},                   // lfd
	{51, &JitLLVM::lfdu},                  // lfdu

	{52, &JitLLVM::stfs},                  // stfs
	{53, &JitLLVM::stfsu},                 // stfsu
	{54, &JitLLVM::stfd},                  // stfd
	{55, &JitLLVM::stfdu},                 // stfdu

	{56, &JitLLVM::psq_lXX},               // psq_l
	{57, &JitLLVM::psq_lXX},               // psq_lu
	{60, &JitLLVM::FallBackToInterpreter}, // psq_st
	{61, &JitLLVM::FallBackToInterpreter}, // psq_stu

	//missing: 0, 1, 2, 5, 6, 9, 22, 30, 62, 58
};

static GekkoOPTemplate table4[] =
{     //SUBOP10
	{0,    &JitLLVM::FallBackToInterpreter},   // ps_cmpu0
	{32,   &JitLLVM::FallBackToInterpreter},   // ps_cmpo0
	{40,   &JitLLVM::ps_neg},                  // ps_neg
	{136,  &JitLLVM::ps_nabs},                 // ps_nabs
	{264,  &JitLLVM::ps_abs},                  // ps_abs
	{64,   &JitLLVM::FallBackToInterpreter},   // ps_cmpu1
	{72,   &JitLLVM::ps_mr},                   // ps_mr
	{96,   &JitLLVM::FallBackToInterpreter},   // ps_cmpo1
	{528,  &JitLLVM::ps_merge00},              // ps_merge00
	{560,  &JitLLVM::ps_merge01},              // ps_merge01
	{592,  &JitLLVM::ps_merge10},              // ps_merge10
	{624,  &JitLLVM::ps_merge11},              // ps_merge11
	{1014, &JitLLVM::FallBackToInterpreter},   // dcbz_l
};

static GekkoOPTemplate table4_2[] =
{
	{10, &JitLLVM::ps_sum0},                   // ps_sum0
	{11, &JitLLVM::ps_sum1},                   // ps_sum1
	{12, &JitLLVM::ps_muls0},                  // ps_muls0
	{13, &JitLLVM::ps_muls1},                  // ps_muls1
	{14, &JitLLVM::ps_madds0},                 // ps_madds0
	{15, &JitLLVM::ps_madds1},                 // ps_madds1
	{18, &JitLLVM::ps_div},                    // ps_div
	{20, &JitLLVM::ps_sub},                    // ps_sub
	{21, &JitLLVM::ps_add},                    // ps_add
	{23, &JitLLVM::ps_sel},                    // ps_sel
	{24, &JitLLVM::ps_res},                    // ps_res
	{25, &JitLLVM::ps_mul},                    // ps_mul
	{26, &JitLLVM::FallBackToInterpreter},     // ps_rsqrte
	{28, &JitLLVM::ps_msub},                   // ps_msub
	{29, &JitLLVM::ps_madd},                   // ps_madd
	{30, &JitLLVM::ps_nmsub},                  // ps_nmsub
	{31, &JitLLVM::ps_nmadd},                  // ps_nmadd
};


static GekkoOPTemplate table4_3[] =
{
	{6,  &JitLLVM::psq_lXX},                   // psq_lx
	{7,  &JitLLVM::FallBackToInterpreter},     // psq_stx
	{38, &JitLLVM::psq_lXX},                   // psq_lux
	{39, &JitLLVM::FallBackToInterpreter},     // psq_stux
};

static GekkoOPTemplate table19[] =
{
	{528, &JitLLVM::bcctrx},                   // bcctrx
	{16,  &JitLLVM::bclrx},                    // bclrx
	{257, &JitLLVM::crXX},                     // crand
	{129, &JitLLVM::crXX},                     // crandc
	{289, &JitLLVM::crXX},                     // creqv
	{225, &JitLLVM::crXX},                     // crnand
	{33,  &JitLLVM::crXX},                     // crnor
	{449, &JitLLVM::crXX},                     // cror
	{417, &JitLLVM::crXX},                     // crorc
	{193, &JitLLVM::crXX},                     // crxor

	{150, &JitLLVM::FallBackToInterpreter},    // isync
	{0,   &JitLLVM::FallBackToInterpreter},    // mcrf

	{50,  &JitLLVM::rfi},                      // rfi
	{18,  &JitLLVM::Break},                    // rfid
};


static GekkoOPTemplate table31[] =
{
	{266,  &JitLLVM::addx},                    // addx
	{778,  &JitLLVM::addx},                    // addox
	{10,   &JitLLVM::addcx},                   // addcx
	{522,  &JitLLVM::addcx},                   // addcox
	{138,  &JitLLVM::addex},                   // addex
	{650,  &JitLLVM::addex},                   // addeox
	{234,  &JitLLVM::addmex},                  // addmex
	{746,  &JitLLVM::addmex},                  // addmeox
	{202,  &JitLLVM::addzex},                  // addzex
	{714,  &JitLLVM::addzex},                  // addzeox
	{491,  &JitLLVM::FallBackToInterpreter},   // divwx
	{1003, &JitLLVM::FallBackToInterpreter},   // divwox
	{459,  &JitLLVM::FallBackToInterpreter},   // divwux
	{971,  &JitLLVM::FallBackToInterpreter},   // divwuox
	{75,   &JitLLVM::mulhwx},                  // mulhwx
	{11,   &JitLLVM::mulhwux},                 // mulhwux
	{235,  &JitLLVM::mullwx},                  // mullwx
	{747,  &JitLLVM::mullwx},                  // mullwox
	{104,  &JitLLVM::negx},                    // negx
	{616,  &JitLLVM::negx},                    // negox
	{40,   &JitLLVM::subfx},                   // subfx
	{552,  &JitLLVM::subfx},                   // subfox
	{8,    &JitLLVM::subfcx},                  // subfcx
	{520,  &JitLLVM::subfcx},                  // subfcox
	{136,  &JitLLVM::subfex},                  // subfex
	{648,  &JitLLVM::subfex},                  // subfeox
	{232,  &JitLLVM::subfmex},                 // subfmex
	{744,  &JitLLVM::subfmex},                 // subfmeox
	{200,  &JitLLVM::subfzex},                 // subfzex
	{712,  &JitLLVM::subfzex},                 // subfzeox

	{28,  &JitLLVM::andx},                     // andx
	{60,  &JitLLVM::andcx},                    // andcx
	{444, &JitLLVM::orx},                      // orx
	{124, &JitLLVM::norx},                     // norx
	{316, &JitLLVM::xorx},                     // xorx
	{412, &JitLLVM::orcx},                     // orcx
	{476, &JitLLVM::nandx},                    // nandx
	{284, &JitLLVM::eqvx},                     // eqvx
	{0,   &JitLLVM::cmp},                      // cmp
	{32,  &JitLLVM::cmpl},                     // cmpl
	{26,  &JitLLVM::cntlzwx},                  // cntlzwx
	{922, &JitLLVM::extsXx},                   // extshx
	{954, &JitLLVM::extsXx},                   // extsbx
	{536, &JitLLVM::srwx},                     // srwx
	{792, &JitLLVM::FallBackToInterpreter},    // srawx
	{824, &JitLLVM::FallBackToInterpreter},    // srawix
	{24,  &JitLLVM::slwx},                     // slwx

	{54,   &JitLLVM::FallBackToInterpreter},   // dcbst
	{86,   &JitLLVM::FallBackToInterpreter},   // dcbf
	{246,  &JitLLVM::DoNothing},               // dcbtst
	{278,  &JitLLVM::DoNothing},               // dcbt
	{470,  &JitLLVM::FallBackToInterpreter},   // dcbi
	{758,  &JitLLVM::DoNothing},               // dcba
	{1014, &JitLLVM::FallBackToInterpreter},   // dcbz

	//load word
	{23,  &JitLLVM::lwzx},                     // lwzx
	{55,  &JitLLVM::lwzux},                    // lwzux

	//load halfword
	{279, &JitLLVM::lhzx},                     // lhzx
	{311, &JitLLVM::lhzux},                    // lhzux

	//load halfword signextend
	{343, &JitLLVM::lhax},                     // lhax
	{375, &JitLLVM::lhaux},                    // lhaux

	//load byte
	{87,  &JitLLVM::lbzx},                     // lbzx
	{119, &JitLLVM::lbzux},                    // lbzux

	//load byte reverse
	{534, &JitLLVM::lwbrx},                    // lwbrx
	{790, &JitLLVM::lhbrx},                    // lhbrx

	// Conditional load/store (Wii SMP)
	{150, &JitLLVM::FallBackToInterpreter},    // stwcxd
	{20,  &JitLLVM::FallBackToInterpreter},    // lwarx

	//load string (interpret these)
	{533, &JitLLVM::FallBackToInterpreter},    // lswx
	{597, &JitLLVM::FallBackToInterpreter},    // lswi

	//store word
	{151, &JitLLVM::stwx},                     // stwx
	{183, &JitLLVM::stwux},                    // stwux

	//store halfword
	{407, &JitLLVM::sthx},                     // sthx
	{439, &JitLLVM::sthux},                    // sthux

	//store byte
	{215, &JitLLVM::stbx},                     // stbx
	{247, &JitLLVM::stbux},                    // stbux

	//store bytereverse
	{662, &JitLLVM::stwbrx},                   // stwbrx
	{918, &JitLLVM::sthbrx},                   // sthbrx

	{661, &JitLLVM::FallBackToInterpreter},    // stswx
	{725, &JitLLVM::FallBackToInterpreter},    // stswi

	// fp load/store
	{535, &JitLLVM::lfsx},                     // lfsx
	{567, &JitLLVM::lfsux},                    // lfsux
	{599, &JitLLVM::lfdx},                     // lfdx
	{631, &JitLLVM::lfdux},                    // lfdux

	{663, &JitLLVM::stfsx},                    // stfsx
	{695, &JitLLVM::stfsux},                   // stfsux
	{727, &JitLLVM::stfdx},                    // stfdx
	{759, &JitLLVM::stfdux},                   // stfdux
	{983, &JitLLVM::stfiwx},                   // stfiwx

	{19,  &JitLLVM::FallBackToInterpreter},    // mfcr
	{83,  &JitLLVM::mfmsr},                    // mfmsr
	{144, &JitLLVM::FallBackToInterpreter},    // mtcrf
	{146, &JitLLVM::mtmsr},                    // mtmsr
	{210, &JitLLVM::mtsr},                     // mtsr
	{242, &JitLLVM::mtsrin},                   // mtsrin
	{339, &JitLLVM::mfspr},                    // mfspr
	{467, &JitLLVM::mtspr},                    // mtspr
	{371, &JitLLVM::FallBackToInterpreter},    // mftb
	{512, &JitLLVM::FallBackToInterpreter},    // mcrxr
	{595, &JitLLVM::mfsr},                     // mfsr
	{659, &JitLLVM::mfsrin},                   // mfsrin

	{4,   &JitLLVM::FallBackToInterpreter},    // tw
	{598, &JitLLVM::DoNothing},                // sync
	{982, &JitLLVM::icbi},                     // icbi

	// Unused instructions on GC
	{310, &JitLLVM::FallBackToInterpreter},    // eciwx
	{438, &JitLLVM::FallBackToInterpreter},    // ecowx
	{854, &JitLLVM::DoNothing},                // eieio
	{306, &JitLLVM::FallBackToInterpreter},    // tlbie
	{370, &JitLLVM::FallBackToInterpreter},    // tlbia
	{566, &JitLLVM::DoNothing},                // tlbsync
};

static GekkoOPTemplate table59[] =
{
	{18, &JitLLVM::FallBackToInterpreter},     // fdivsx
	{20, &JitLLVM::fsubsx},                    // fsubsx
	{21, &JitLLVM::faddsx},                    // faddsx
//  {22, &JitLLVM::FallBackToInterpreter},       // fsqrtsx
	{24, &JitLLVM::FallBackToInterpreter},     // fresx
	{25, &JitLLVM::fmulsx},                    // fmulsx
	{28, &JitLLVM::fmsubsx},                   // fmsubsx
	{29, &JitLLVM::fmaddsx},                   // fmaddsx
	{30, &JitLLVM::fnmsubsx},                  // fnmsubsx
	{31, &JitLLVM::fnmaddsx},                  // fnmaddsx
};

static GekkoOPTemplate table63[] =
{
	{264, &JitLLVM::fabsx},                    // fabsx
	{32,  &JitLLVM::FallBackToInterpreter},    // fcmpo
	{0,   &JitLLVM::FallBackToInterpreter},    // fcmpu
	{14,  &JitLLVM::FallBackToInterpreter},    // fctiwx
	{15,  &JitLLVM::FallBackToInterpreter},    // fctiwzx
	{72,  &JitLLVM::fmrx},                     // fmrx
	{136, &JitLLVM::fnabsx},                   // fnabsx
	{40,  &JitLLVM::fnegx},                    // fnegx
	{12,  &JitLLVM::FallBackToInterpreter},    // frspx

	{64,  &JitLLVM::FallBackToInterpreter},    // mcrfs
	{583, &JitLLVM::FallBackToInterpreter},    // mffsx
	{70,  &JitLLVM::FallBackToInterpreter},    // mtfsb0x
	{38,  &JitLLVM::FallBackToInterpreter},    // mtfsb1x
	{134, &JitLLVM::FallBackToInterpreter},    // mtfsfix
	{711, &JitLLVM::FallBackToInterpreter},    // mtfsfx
};

static GekkoOPTemplate table63_2[] =
{
	{18, &JitLLVM::FallBackToInterpreter},     // fdivx
	{20, &JitLLVM::fsubx},                     // fsubx
	{21, &JitLLVM::faddx},                     // faddx
	{22, &JitLLVM::FallBackToInterpreter},     // fsqrtx
	{23, &JitLLVM::fselx},                     // fselx
	{25, &JitLLVM::fmulx},                     // fmulx
	{26, &JitLLVM::FallBackToInterpreter},     // frsqrtex
	{28, &JitLLVM::fmsubx},                    // fmsubx
	{29, &JitLLVM::fmaddx},                    // fmaddx
	{30, &JitLLVM::fnmsubx},                   // fnmsubx
	{31, &JitLLVM::fnmaddx},                   // fnmaddx
};
namespace JitLLVMTables
{

void CompileInstruction(JitLLVM* _jit, LLVMFunction* func, PPCAnalyst::CodeOp& op)
{
	(_jit->*dynaOpTable[op.inst.OPCD])(func, op.inst);
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
		tpl = &JitLLVM::FallBackToInterpreter;
	}

	for (auto& tpl : dynaOpTable59)
	{
		tpl = &JitLLVM::FallBackToInterpreter;
	}

	for (int i = 0; i < 1024; i++)
	{
		dynaOpTable4 [i] = &JitLLVM::FallBackToInterpreter;
		dynaOpTable19[i] = &JitLLVM::FallBackToInterpreter;
		dynaOpTable31[i] = &JitLLVM::FallBackToInterpreter;
		dynaOpTable63[i] = &JitLLVM::FallBackToInterpreter;
	}

	for (auto& tpl : primarytable)
	{
		dynaOpTable[tpl.opcode] = tpl.Inst;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (auto& tpl : table4_2)
		{
			int op = fill+tpl.opcode;
			dynaOpTable4[op] = tpl.Inst;
		}
	}

	for (int i = 0; i < 16; i++)
	{
		int fill = i << 6;
		for (auto& tpl : table4_3)
		{
			int op = fill+tpl.opcode;
			dynaOpTable4[op] = tpl.Inst;
		}
	}

	for (auto& tpl : table4)
	{
		int op = tpl.opcode;
		dynaOpTable4[op] = tpl.Inst;
	}

	for (auto& tpl : table31)
	{
		int op = tpl.opcode;
		dynaOpTable31[op] = tpl.Inst;
	}

	for (auto& tpl : table19)
	{
		int op = tpl.opcode;
		dynaOpTable19[op] = tpl.Inst;
	}

	for (auto& tpl : table59)
	{
		int op = tpl.opcode;
		dynaOpTable59[op] = tpl.Inst;
	}

	for (auto& tpl : table63)
	{
		int op = tpl.opcode;
		dynaOpTable63[op] = tpl.Inst;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (auto& tpl : table63_2)
		{
			int op = fill + tpl.opcode;
			dynaOpTable63[op] = tpl.Inst;
		}
	}

	initialized = true;
}

}  // namespace
