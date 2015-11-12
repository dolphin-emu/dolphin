/*
** Definitions for ARM64 CPUs.
** Copyright (C) 2005-2015 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_TARGET_ARM64_H
#define _LJ_TARGET_ARM64_H

/* -- Registers IDs ------------------------------------------------------- */

#define GPRDEF(_) \
  _(X0) _(X1) _(X2) _(X3) _(X4) _(X5) _(X6) _(X7) \
  _(X8) _(X9) _(X10) _(X11) _(X12) _(X13) _(X14) _(X15) \
  _(X16) _(X17) _(X18) _(X19) _(X20) _(X21) _(X22) _(X23) \
  _(X24) _(X25) _(X26) _(X27) _(X28) _(FP) _(LR) _(SP)
#define FPRDEF(_) \
  _(D0) _(D1) _(D2) _(D3) _(D4) _(D5) _(D6) _(D7) \
  _(D8) _(D9) _(D10) _(D11) _(D12) _(D13) _(D14) _(D15) \
  _(D16) _(D17) _(D18) _(D19) _(D20) _(D21) _(D22) _(D23) \
  _(D24) _(D25) _(D26) _(D27) _(D28) _(D29) _(D30) _(D31)
#define VRIDDEF(_)

#define RIDENUM(name)	RID_##name,

enum {
  GPRDEF(RIDENUM)		/* General-purpose registers (GPRs). */
  FPRDEF(RIDENUM)		/* Floating-point registers (FPRs). */
  RID_MAX,
  RID_TMP = RID_LR,
  RID_ZERO = RID_SP,

  /* Calling conventions. */
  RID_RET = RID_X0,
  RID_FPRET = RID_D0,

  /* These definitions must match with the *.dasc file(s): */
  RID_BASE = RID_X19,		/* Interpreter BASE. */
  RID_LPC = RID_X21,		/* Interpreter PC. */
  RID_GL = RID_X22,		/* Interpreter GL. */
  RID_LREG = RID_X23,		/* Interpreter L. */

  /* Register ranges [min, max) and number of registers. */
  RID_MIN_GPR = RID_X0,
  RID_MAX_GPR = RID_SP+1,
  RID_MIN_FPR = RID_MAX_GPR,
  RID_MAX_FPR = RID_D31+1,
  RID_NUM_GPR = RID_MAX_GPR - RID_MIN_GPR,
  RID_NUM_FPR = RID_MAX_FPR - RID_MIN_FPR
};

#define RID_NUM_KREF		RID_NUM_GPR
#define RID_MIN_KREF		RID_X0

/* -- Register sets ------------------------------------------------------- */

/* Make use of all registers, except for x18, fp, lr and sp. */
#define RSET_FIXED \
  (RID2RSET(RID_X18)|RID2RSET(RID_FP)|RID2RSET(RID_LR)|RID2RSET(RID_SP))
#define RSET_GPR	(RSET_RANGE(RID_MIN_GPR, RID_MAX_GPR) - RSET_FIXED)
#define RSET_FPR	RSET_RANGE(RID_MIN_FPR, RID_MAX_FPR)
#define RSET_ALL	(RSET_GPR|RSET_FPR)
#define RSET_INIT	RSET_ALL

/* lr is an implicit scratch register. */
#define RSET_SCRATCH_GPR	(RSET_RANGE(RID_X0, RID_X17+1))
#define RSET_SCRATCH_FPR \
  (RSET_RANGE(RID_D0, RID_D7+1)|RSET_RANGE(RID_D16, RID_D31+1))
#define RSET_SCRATCH		(RSET_SCRATCH_GPR|RSET_SCRATCH_FPR)
#define REGARG_FIRSTGPR		RID_X0
#define REGARG_LASTGPR		RID_X7
#define REGARG_NUMGPR		8
#define REGARG_FIRSTFPR		RID_D0
#define REGARG_LASTFPR		RID_D7
#define REGARG_NUMFPR		8

/* -- Instructions -------------------------------------------------------- */

/* Instruction fields. */
#define A64F_D(r)	(r)
#define A64F_N(r)       ((r) << 5)
#define A64F_A(r)       ((r) << 10)
#define A64F_M(r)       ((r) << 16)
#define A64F_U16(x)	((x) << 5)
#define A64F_S26(x)	(x)
#define A64F_S19(x)	((x) << 5)

typedef enum A64Ins {
  A64I_MOVZw = 0x52800000,
  A64I_MOVZx = 0xd2800000,
  A64I_LDRLw = 0x18000000,
  A64I_LDRLx = 0x58000000,
  A64I_NOP = 0xd503201f,
  A64I_B = 0x14000000,
  A64I_BR = 0xd61f0000,
} A64Ins;

#endif
