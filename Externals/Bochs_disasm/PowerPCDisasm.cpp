/* $VER: ppc_disasm.c V1.1 (19.02.2000)
*
* Disassembler module for the PowerPC microprocessor family
* Copyright (c) 1998-2000  Frank Wille
*
* ppc_disasm.c is freeware and may be freely redistributed as long as
* no modifications are made and nothing is charged for it.
* Non-commercial usage is allowed without any restrictions.
* EVERY PRODUCT OR PROGRAM DERIVED DIRECTLY FROM MY SOURCE MAY NOT BE
* SOLD COMMERCIALLY WITHOUT PERMISSION FROM THE AUTHOR.
*
*
* v1.2  (31.07.2003) org
*       modified for IBM PowerPC Gekko.
* v1.1  (19.02.2000) phx
*       fabs wasn't recognized.
* v1.0  (30.01.2000) phx
*       stfsx, stfdx, lfsx, lfdx, stfsux, stfdux, lfsux, lfdux, etc.
*       printed "rd,ra,rb" as operands instead "fd,ra,rb".
* v0.4  (01.06.1999) phx
*       'stwm' shoud have been 'stmw'.
* v0.3  (17.11.1998) phx
*       The OE-types (e.g. addo, subfeo, etc.) didn't work for all
*       instructions.
*       AA-form branches have an absolute destination.
*       addze and subfze must not have a third operand.
*       sc was not recognized.
* v0.2  (29.05.1998) phx
*       Sign error. SUBI got negative immediate values.
* v0.1  (23.05.1998) phx
*       First version, which implements all PowerPC instructions.
* v0.0  (09.05.1998) phx
*       File created.
*/
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "PowerPCDisasm.h"

namespace PPCDisasm
{

/* version/revision */
#define PPCDISASM_VER 1
#define PPCDISASM_REV 1


/* typedefs */
typedef unsigned int ppc_word;

#undef BIGENDIAN
#undef LITTTLEENDIAN
/* endianess */
#define LITTLEENDIAN 0


/* general defines */
#define PPCIDXMASK      0xfc000000
#define PPCIDX2MASK     0x000007fe
#define PPCDMASK        0x03e00000
#define PPCAMASK        0x001f0000
#define PPCBMASK        0x0000f800
#define PPCCMASK        0x000007c0
#define PPCMMASK        0x0000003e
#define PPCCRDMASK      0x03800000
#define PPCCRAMASK      0x001c0000
#define PPCLMASK        0x00600000
#define PPCOE           0x00000400

#define PPCIDXSH        26
#define PPCDSH          21
#define PPCASH          16
#define PPCBSH          11
#define PPCCSH          6
#define PPCMSH          1
#define PPCCRDSH        23
#define PPCCRASH        18
#define PPCLSH          21
#define PPCIDX2SH       1

#define PPCGETIDX(x)    (((x)&PPCIDXMASK)>>PPCIDXSH)
#define PPCGETD(x)      (((x)&PPCDMASK)>>PPCDSH)
#define PPCGETA(x)      (((x)&PPCAMASK)>>PPCASH)
#define PPCGETB(x)      (((x)&PPCBMASK)>>PPCBSH)
#define PPCGETC(x)      (((x)&PPCCMASK)>>PPCCSH)
#define PPCGETM(x)      (((x)&PPCMMASK)>>PPCMSH)
#define PPCGETCRD(x)    (((x)&PPCCRDMASK)>>PPCCRDSH)
#define PPCGETCRA(x)    (((x)&PPCCRAMASK)>>PPCCRASH)
#define PPCGETL(x)      (((x)&PPCLMASK)>>PPCLSH)
#define PPCGETIDX2(x)   (((x)&PPCIDX2MASK)>>PPCIDX2SH)


	/* Disassembler structure, the interface to the application */

	struct DisasmPara_PPC {
		ppc_word *instr;              /* pointer to instruction to disassemble */
		ppc_word *iaddr;              /* instr.addr., usually the same as instr */
		char *opcode;                 /* buffer for opcode, min. 10 chars. */
		char *operands;               /* operand buffer, min. 24 chars. */
		/* changed by disassembler: */
		unsigned char type;           /* type of instruction, see below */
		unsigned char flags;          /* additional flags */
		unsigned short sreg;          /* register in load/store instructions */
		ppc_word displacement;        /* branch- or load/store displacement */
	};

#define PPCINSTR_OTHER      0   /* no additional info for other instr. */
#define PPCINSTR_BRANCH     1   /* branch dest. = PC+displacement */
#define PPCINSTR_LDST       2   /* load/store instruction: displ(sreg) */
#define PPCINSTR_IMM        3   /* 16-bit immediate val. in displacement */

#define PPCF_ILLEGAL   (1<<0)   /* illegal PowerPC instruction */
#define PPCF_UNSIGNED  (1<<1)   /* unsigned immediate instruction */
#define PPCF_SUPER     (1<<2)   /* supervisor level instruction */
#define PPCF_64        (1<<3)   /* 64-bit only instruction */


	/* ppc_disasm.o prototypes */
#ifndef PPC_DISASM_C
	extern ppc_word *PPC_Disassemble(struct DisasmPara_PPC *);
#endif


	static const char *trap_condition[32] = {
		NULL,"lgt","llt",NULL,"eq","lge","lle",NULL,
		"gt",NULL,NULL,NULL,"ge",NULL,NULL,NULL,
		"lt",NULL,NULL,NULL,"le",NULL,NULL,NULL,
		"ne",NULL,NULL,NULL,NULL,NULL,NULL,NULL
	};

	static const char *cmpname[4] = {
		"cmpw","cmpd","cmplw","cmpld"
	};

	static const char *b_ext[4] = {
		"","l","a","la"
	};

	static const char *b_condition[8] = {
		"ge","le","ne","ns","lt","gt","eq","so"
	};

	static const char *b_decr[16] = {
		"nzf","zf",NULL,NULL,"nzt","zt",NULL,NULL,
		"nz","z",NULL,NULL,"nz","z",NULL,NULL
	};

	static const char *regsel[2] = {
		"","r"
	};

	static const char *oesel[2] = {
		"","o"
	};

	static const char *rcsel[2] = {
		"","."
	};

	static const char *ldstnames[] = {
		"lwz","lwzu","lbz","lbzu","stw","stwu","stb","stbu","lhz","lhzu",
		"lha","lhau","sth","sthu","lmw","stmw","lfs","lfsu","lfd","lfdu",
		"stfs","stfsu","stfd","stfdu"
	};

	static const char *regnames[] = {
		"r0", "sp", "rtoc", "r3", "r4", "r5", "r6", "r7", 
		"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", 
		"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", 
		"r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
	};

	static const char *spr_name(int i)
	{
		static char def[8];

		switch (i)
		{
		case 1: return "XER";
		case 8: return "LR";
		case 9: return "CTR";
		case 18: return "DSIR";
		case 19: return "DAR";
		case 22: return "DEC";
		case 25: return "SDR1";
		case 26: return "SRR0";
		case 27: return "SRR1";
		case 272: return "SPRG0";
		case 273: return "SPRG1";
		case 274: return "SPRG2";
		case 275: return "SPRG3";
		case 282: return "EAR";
		case 287: return "PVR";
		case 528: return "IBAT0U";
		case 529: return "IBAT0L";
		case 530: return "IBAT1U";
		case 531: return "IBAT1L";
		case 532: return "IBAT2U";
		case 533: return "IBAT2L";
		case 534: return "IBAT3U";
		case 535: return "IBAT3L";
		case 536: return "DBAT0U";
		case 537: return "DBAT0L";
		case 538: return "DBAT1U";
		case 539: return "DBAT1L";
		case 540: return "DBAT2U";
		case 541: return "DBAT2L";
		case 542: return "DBAT3U";
		case 543: return "DBAT3L";
		case 912: return "GQR0";
		case 913: return "GQR1";
		case 914: return "GQR2";
		case 915: return "GQR3";
		case 916: return "GQR4";
		case 917: return "GQR5";
		case 918: return "GQR6";
		case 919: return "GQR7";
		case 920: return "HID2";
		case 921: return "WPAR";
		case 922: return "DMA_U";
		case 923: return "DMA_L";
		case 924: return "ECID_U";
		case 925: return "ECID_M";
		case 926: return "ECID_L";
		case 936: return "UMMCR0";
		case 937: return "UPMC1";
		case 938: return "UPMC2";
		case 939: return "USIA";
		case 940: return "UMMCR1";
		case 941: return "UPMC3";
		case 942: return "UPMC4";
		case 943: return "USDA";
		case 952: return "MMCR0";
		case 953: return "PMC1";
		case 954: return "PMC2";
		case 955: return "SIA";
		case 956: return "MMCR1";
		case 957: return "PMC3";
		case 958: return "PMC4";
		case 959: return "SDA";
		case 1008: return "HID0";
		case 1009: return "HID1";
		case 1010: return "IABR";
		case 1011: return "HID4";
		case 1013: return "DABR";
		case 1017: return "L2CR";
		case 1019: return "ICTC";
		case 1020: return "THRM1";
		case 1021: return "THRM2";
		case 1022: return "THRM3";
		}

		sprintf(def, "%i", i);
		return def;
	}

	static void ierror(const char *errtxt,...)
		/* display internal error and quit program */
	{
		va_list vl;

		fprintf(stderr,"\nINTERNAL ERROR (PPC disassembler): ");
		va_start(vl,errtxt);
		vfprintf(stderr,errtxt,vl);
		va_end(vl);
		fprintf(stderr,".\nAborting.\n");
		exit(1);
	}


	static ppc_word swapda(ppc_word w)
	{
		return ((w&0xfc00ffff)|((w&PPCAMASK)<<5)|((w&PPCDMASK)>>5));
	}


	static ppc_word swapab(ppc_word w)
	{
		return ((w&0xffe007ff)|((w&PPCBMASK)<<5)|((w&PPCAMASK)>>5));
	}


	static void ill(struct DisasmPara_PPC *dp,ppc_word in)
	{
		if (in == 0) {
			strcpy(dp->opcode, "");
			strcpy(dp->operands, "---");
		} else {
			strcpy(dp->opcode, "( ill )");
			sprintf(dp->operands, "%08x", in);
		}

		dp->flags |= PPCF_ILLEGAL;
	}


	static void imm(struct DisasmPara_PPC *dp,ppc_word in,int uimm,int type,int hex)
		/* Generate immediate instruction operand. */
		/* type 0: D-mode, D,A,imm */
		/* type 1: S-mode, A,S,imm */
		/* type 2: S/D register is ignored (trap,cmpi) */
		/* type 3: A register is ignored (li) */
	{
		int i = (int)(in & 0xffff);

		dp->type = PPCINSTR_IMM;
		if (!uimm) {
			if (i > 0x7fff)
				i -= 0x10000;
		}
		else
			dp->flags |= PPCF_UNSIGNED;
		dp->displacement = i;

		switch (type) {
	case 0:
		sprintf(dp->operands,"%s, %s, %d",regnames[(int)PPCGETD(in)],regnames[(int)PPCGETA(in)],i);
		break;
	case 1:
		if (hex)
			sprintf(dp->operands,"%s, %s, 0x%.4X",regnames[(int)PPCGETA(in)],regnames[(int)PPCGETD(in)],i);
		else
			sprintf(dp->operands,"%s, %s, %d",regnames[(int)PPCGETA(in)],regnames[(int)PPCGETD(in)],i);
		break;
	case 2:
		sprintf(dp->operands,"%s, %d",regnames[(int)PPCGETA(in)],i);
		break;
	case 3:
		if (hex)
			sprintf(dp->operands,"%s, 0x%.4X",regnames[(int)PPCGETD(in)],i);
		else
			sprintf(dp->operands,"%s, %d",regnames[(int)PPCGETD(in)],i);
		break;
	default:
		ierror("imm(): Wrong type");
		break;
		}
	}


	static void ra_rb(char *s,ppc_word in)
	{
		sprintf(s,"%s, %s",regnames[(int)PPCGETA(in)],regnames[(int)PPCGETB(in)]);
	}


	static char *rd_ra_rb(char *s,ppc_word in,int mask)
	{
		static const char *fmt = "%s, ";

		if (mask) {
			if (mask & 4)
				s += sprintf(s,fmt,regnames[(int)PPCGETD(in)]);
			if (mask & 2)
				s += sprintf(s,fmt,regnames[(int)PPCGETA(in)]);
			if (mask & 1)
				s += sprintf(s,fmt,regnames[(int)PPCGETB(in)]);
			*--s = '\0';
			*--s = '\0';
		}
		else
			*s = '\0';
		return (s);
	}


	static char *fd_ra_rb(char *s,ppc_word in,int mask)
	{
		static const char *ffmt = "f%d,";
		static const char *rfmt = "%s,";

		if (mask) {
			if (mask & 4)
				s += sprintf(s,ffmt,(int)PPCGETD(in));
			if (mask & 2)
				s += sprintf(s,rfmt,regnames[(int)PPCGETA(in)]);
			if (mask & 1)
				s += sprintf(s,rfmt,regnames[(int)PPCGETB(in)]);
			*--s = '\0';
		}
		else
			*s = '\0';
		return (s);
	}


	static void trapi(struct DisasmPara_PPC *dp,ppc_word in,unsigned char dmode)
	{
		const char *cnd;

		if ((cnd = trap_condition[PPCGETD(in)]) != NULL) {
			dp->flags |= dmode;
			sprintf(dp->opcode,"t%c%s",dmode?'d':'w',cnd);
			imm(dp,in,0,2,0);
		}
		else
			ill(dp,in);
	}


	static void cmpi(struct DisasmPara_PPC *dp,ppc_word in,int uimm)
	{
		char *oper = dp->operands;
		int i = (int)PPCGETL(in);

		if (i < 2) {
			if (i)
				dp->flags |= PPCF_64;
			sprintf(dp->opcode,"%si",cmpname[uimm*2+i]);
			if ((i = (int)PPCGETCRD(in))) {
				sprintf(oper,"cr%c,",'0'+i);
				dp->operands += 4;
			}
			imm(dp,in,uimm,2,0);
			dp->operands = oper;
		}
		else
			ill(dp,in);
	}


	static void addi(struct DisasmPara_PPC *dp,ppc_word in,const char *ext)
	{
		if ((in&0x08000000) && !PPCGETA(in)) {
			sprintf(dp->opcode,"l%s",ext);  /* li, lis */
			if (!strcmp(ext, "i"))
				imm(dp,in,0,3,0);
			else
				imm(dp,in,1,3,1);
		}
		else {
			sprintf(dp->opcode,"%s%s",(in&0x8000)?"sub":"add",ext);
			if (in & 0x8000)
				in = (in^0xffff) + 1;
			imm(dp,in,1,0,0);
		}
	}


	static int branch(struct DisasmPara_PPC *dp,ppc_word in, const char *bname,int aform,int bdisp)
		/* build a branch instr. and return number of chars written to operand */
	{
		int bo = (int)PPCGETD(in);
		int bi = (int)PPCGETA(in);
		char y = (char)(bo & 1);
		int opercnt = 0;
		const 	char *ext = b_ext[aform*2+(int)(in&1)];

		if (bdisp < 0)
			y ^= 1;
		y = y ? '+':'-';

		if (bo & 4) {
			/* standard case - no decrement */
			if (bo & 16) {
				/* branch always */
				if (PPCGETIDX(in) != 16) {
					sprintf(dp->opcode,"b%s%s",bname,ext);
				}
				else {
					sprintf(dp->opcode,"bc%s",ext);
					opercnt = sprintf(dp->operands,"%d, %d",bo,bi);
				}
			}
			else {
				/* branch conditional */
				sprintf(dp->opcode,"b%s%s%s%c",b_condition[((bo&8)>>1)+(bi&3)],
					bname,ext,y);
				if (bi >= 4)
					opercnt = sprintf(dp->operands,"cr%d",bi>>2);
			}
		}

		else {
			/* CTR is decremented and checked */
			sprintf(dp->opcode,"bd%s%s%s%c",b_decr[bo>>1],bname,ext,y);
			if (!(bo & 16))
				opercnt = sprintf(dp->operands,"%d",bi);
		}

		return (opercnt);
	}


	static void bc(struct DisasmPara_PPC *dp,ppc_word in)
	{
		unsigned int d = (int)(in & 0xfffc);
		int offs;
		char *oper = dp->operands;

		if (d & 0x8000) d |= 0xffff0000;

		if ((offs = branch(dp,in,"",(in&2)?1:0,d))) {
			oper += offs;
			*oper++ = ',';
		}
		if (in & 2)  /* AA ? */
			sprintf(dp->operands,"->0x%.8X",(unsigned int)d);
		else
			sprintf(oper,"->0x%.8X",(unsigned int)(*dp->iaddr) + d);
		dp->type = PPCINSTR_BRANCH;
		dp->displacement = (ppc_word)d;
	}


	static void bli(struct DisasmPara_PPC *dp,ppc_word in)
	{
		unsigned int d = (unsigned int)(in & 0x3fffffc);

		if (d & 0x02000000) d |= 0xfc000000;

		sprintf(dp->opcode,"b%s",b_ext[in&3]);
		if (in & 2)  /* AA ? */
			sprintf(dp->operands,"->0x%.8X",(unsigned int)d);
		else
			sprintf(dp->operands,"->0x%.8X",(unsigned int)(*dp->iaddr) + d);
		dp->type = PPCINSTR_BRANCH;
		dp->displacement = (ppc_word)d;
	}


	static void mcrf(struct DisasmPara_PPC *dp,ppc_word in,char c)
	{
		if (!(in & 0x0063f801)) {
			sprintf(dp->opcode,"mcrf%c",c);
			sprintf(dp->operands,"cr%d, cr%d",(int)PPCGETCRD(in),(int)PPCGETCRA(in));
		}
		else
			ill(dp,in);
	}


	static void crop(struct DisasmPara_PPC *dp,ppc_word in,const char *n1,const char *n2)
	{
		int crd = (int)PPCGETD(in);
		int cra = (int)PPCGETA(in);
		int crb = (int)PPCGETB(in);

		if (!(in & 1)) {
			sprintf(dp->opcode,"cr%s",(cra==crb && n2)?n2:n1);
			if (cra == crb && n2)
				sprintf(dp->operands,"%d, %d",crd,cra);
			else
				sprintf(dp->operands,"%d, %d, %d",crd,cra,crb);
		}
		else
			ill(dp,in);
	}


	static void nooper(struct DisasmPara_PPC *dp,ppc_word in,const char *name,
		unsigned char dmode)
	{
		if (in & (PPCDMASK|PPCAMASK|PPCBMASK|1)) {
			ill(dp,in);
		}
		else {
			dp->flags |= dmode;
			strcpy(dp->opcode,name);
		}
	}


	static unsigned int Helper_Rotate_Mask(int r, int mb, int me)
	{
		//first make 001111111111111 part
		unsigned int begin = 0xFFFFFFFF >> mb;
		//then make 000000000001111 part, which is used to flip the bits of the first one
		unsigned int end = me < 31 ? (0xFFFFFFFF >> (me + 1)) : 0;
		//do the bitflip
		unsigned int mask = begin ^ end;
		//and invert if backwards
		if (me < mb) 
			mask = ~mask;
		//rotate the mask so it can be applied to source reg
		//return _rotl(mask, 32 - r);
		return (mask << (32 - r)) | (mask >> r);
	}


	static void rlw(struct DisasmPara_PPC *dp,ppc_word in,const char *name,int i)
	{
		int s = (int)PPCGETD(in);
		int a = (int)PPCGETA(in);
		int bsh = (int)PPCGETB(in);
		int mb = (int)PPCGETC(in);
		int me = (int)PPCGETM(in);
		sprintf(dp->opcode,"rlw%s%c",name,in&1?'.':'\0');
		sprintf(dp->operands,"%s, %s, %s%d, %d, %d (%08x)",regnames[a],regnames[s],regsel[i],bsh,mb,me,Helper_Rotate_Mask(bsh, mb, me));
	}


	static void ori(struct DisasmPara_PPC *dp,ppc_word in,const char *name)
	{
		strcpy(dp->opcode,name);
		imm(dp,in,1,1,1);
	}


	static void rld(struct DisasmPara_PPC *dp,ppc_word in,const char *name,int i)
	{
		int s = (int)PPCGETD(in);
		int a = (int)PPCGETA(in);
		int bsh = i ? (int)PPCGETB(in) : (int)(((in&2)<<4)+PPCGETB(in));
		int m = (int)(in&0x7e0)>>5;

		dp->flags |= PPCF_64;
		sprintf(dp->opcode,"rld%s%c",name,in&1?'.':'\0');
		sprintf(dp->operands,"%s, %s, %s%d, %d",regnames[a],regnames[s],regsel[i],bsh,m);
	}


	static void cmp(struct DisasmPara_PPC *dp,ppc_word in)
	{
		char *oper = dp->operands;
		int i = (int)PPCGETL(in);

		if (i < 2) {
			if (i)
				dp->flags |= PPCF_64;
			strcpy(dp->opcode,cmpname[((in&PPCIDX2MASK)?2:0)+i]);
			if ((i = (int)PPCGETCRD(in)))
				oper += sprintf(oper,"cr%c,",'0'+i);
			ra_rb(oper,in);
		}
		else
			ill(dp,in);
	}


	static void trap(struct DisasmPara_PPC *dp,ppc_word in,unsigned char dmode)
	{
		const char *cnd;
		int to = (int)PPCGETD(in);

		if ((cnd = trap_condition[to])) {
			dp->flags |= dmode;
			sprintf(dp->opcode,"t%c%s",dmode?'d':'w',cnd);
			ra_rb(dp->operands,in);
		}
		else {
			if (to == 31) {
				if (dmode) {
					dp->flags |= dmode;
					strcpy(dp->opcode,"td");
					strcpy(dp->operands,"31,0,0");
				}
				else
					strcpy(dp->opcode,"trap");
			}
			else
				ill(dp,in);
		}
	}


	static void dab(struct DisasmPara_PPC *dp,ppc_word in,const char *name,int mask,
		int smode,int chkoe,int chkrc,unsigned char dmode)
		/* standard instruction: xxxx rD,rA,rB */
	{
		if (chkrc>=0 && ((in&1)!=(unsigned)chkrc)) {
			ill(dp,in);
		}
		else {
			dp->flags |= dmode;
			if (smode)
				in = swapda(in);  /* rA,rS,rB */
			sprintf(dp->opcode,"%s%s%s",name,
				oesel[chkoe&&(in&PPCOE)],rcsel[(chkrc<0)&&(in&1)]);
			rd_ra_rb(dp->operands,in,mask);
		}
	}


	static void rrn(struct DisasmPara_PPC *dp,ppc_word in,const char *name,
		int smode,int chkoe,int chkrc,unsigned char dmode)
		/* Last operand is no register: xxxx rD,rA,NB */
	{
		char *s;

		if (chkrc>=0 && ((in&1)!=(unsigned)chkrc)) {
			ill(dp,in);
		}
		else {
			dp->flags |= dmode;
			if (smode)
				in = swapda(in);  /* rA,rS,NB */
			sprintf(dp->opcode,"%s%s%s",name,
				oesel[chkoe&&(in&PPCOE)],rcsel[(chkrc<0)&&(in&1)]);
			s = rd_ra_rb(dp->operands,in,6);
			sprintf(s,",%d",(int)PPCGETB(in));
		}
	}


	static void mtcr(struct DisasmPara_PPC *dp,ppc_word in)
	{
		int s = (int)PPCGETD(in);
		int crm = (int)(in&0x000ff000)>>12;
		char *oper = dp->operands;

		if (in & 0x00100801) {
			ill(dp,in);
		}
		else {
			sprintf(dp->opcode,"mtcr%c",crm==0xff?'\0':'f');
			if (crm != 0xff)
				oper += sprintf(oper,"0x%02x,",crm);
			sprintf(oper,"%s",regnames[s]);
		}
	}


	static void msr(struct DisasmPara_PPC *dp,ppc_word in,int smode)
	{
		int s = (int)PPCGETD(in);
		int sr = (int)(in&0x000f0000)>>16;

		if (in & 0x0010f801) {
			ill(dp,in);
		}
		else {
			dp->flags |= PPCF_SUPER;
			sprintf(dp->opcode,"m%csr",smode?'t':'f');
			if (smode)
				sprintf(dp->operands,"%d, %s",sr,regnames[s]);
			else
				sprintf(dp->operands,"%s, %d",regnames[s],sr);
		}
	}


	static void mspr(struct DisasmPara_PPC *dp,ppc_word in,int smode)
	{
		int d = (int)PPCGETD(in);
		int spr = (int)((PPCGETB(in)<<5)+PPCGETA(in));
		int fmt = 0;
		const char *x;

		if (in & 1) {
			ill(dp,in);
		}

		else {
			if (spr!=1 && spr!=8 && spr!=9)
				dp->flags |= PPCF_SUPER;
			switch (spr) {
			case 1:
				x = "xer";
				break;
			case 8:
				x = "lr";
				break;
			case 9:
				x = "ctr";
				break;
			default:
				x = "spr";
				fmt = 1;
				break;
			}

			sprintf(dp->opcode,"m%c%s",smode?'t':'f',x);
			if (fmt) {
				if (smode)
					sprintf(dp->operands,"%s, %s",spr_name(spr),regnames[d]);
				else
					sprintf(dp->operands,"%s, %s",regnames[d],spr_name(spr));
			}
			else
				sprintf(dp->operands,"%s",regnames[d]);
		}
	}


	static void mtb(struct DisasmPara_PPC *dp,ppc_word in)
	{
		int d = (int)PPCGETD(in);
		int tbr = (int)((PPCGETB(in)<<5)+PPCGETA(in));
		char *s = dp->operands;
		char x;

		if (in & 1) {
			ill(dp,in);
		}

		else {
			s += sprintf(s,"%s",regnames[d]);
			switch (tbr) {
	case 268:
		x = 'l';
		break;
	case 269:
		x = 'u';
		break;
	default:
		x = '\0';
		dp->flags |= PPCF_SUPER;
		sprintf(s,",%d",tbr);
		break;
			}
			sprintf(dp->opcode,"mftb%c",x);
		}
	}


	static void sradi(struct DisasmPara_PPC *dp,ppc_word in)
	{
		int s = (int)PPCGETD(in);
		int a = (int)PPCGETA(in);
		int bsh = (int)(((in&2)<<4)+PPCGETB(in));

		dp->flags |= PPCF_64;
		sprintf(dp->opcode,"sradi%c",in&1?'.':'\0');
		sprintf(dp->operands,"%s, %s, %d",regnames[a],regnames[s],bsh);
	}

	static const char *ldst_offs(unsigned int val)
	{
		static char buf[8];

		if (val == 0)
		{
			return "0";
		}
		else
		{
			if (val & 0x8000)
			{
				sprintf(buf, "-0x%.4X", ((~val) & 0xffff) + 1);
			}
			else
			{
				sprintf(buf, "0x%.4X", val);
			}

			return buf;
		}
	}

	static void ldst(struct DisasmPara_PPC *dp,ppc_word in,const char *name,
		char reg,unsigned char dmode)
	{
		int s = (int)PPCGETD(in);
		int a = (int)PPCGETA(in);
		int d = (ppc_word)(in & 0xffff);

		dp->type = PPCINSTR_LDST;
		dp->flags |= dmode;
		dp->sreg = (short)a;
		//  if (d >= 0x8000)
		//    d -= 0x10000;
		dp->displacement = (ppc_word)d;
		strcpy(dp->opcode,name);
		if (reg == 'r')
		{
			sprintf(dp->operands,"%s, %s (%s)", regnames[s], ldst_offs(d), regnames[a]);
		}
		else
		{
			sprintf(dp->operands,"%c%d, %s (%s)",reg,s, ldst_offs(d), regnames[a]);
		}
	}


	static void fdabc(struct DisasmPara_PPC *dp,ppc_word in, const char *name,
		int mask,unsigned char dmode)
		/* standard floating point instruction: xxxx fD,fA,fC,fB */
	{
		static const char *fmt = "f%d,";
		char *s = dp->operands;
		int err = 0;

		dp->flags |= dmode;
		sprintf(dp->opcode,"f%s%s",name,rcsel[in&1]);
		s += sprintf(s,fmt,(int)PPCGETD(in));
		if (mask & 4)
			s += sprintf(s,fmt,(int)PPCGETA(in));
		else
			err |= (int)PPCGETA(in);
		if (mask & 2)
			s += sprintf(s,fmt,(int)PPCGETC(in));
		else if (PPCGETC(in))
			err |= (int)PPCGETC(in);
		if (mask & 1)
			s += sprintf(s,fmt,(int)PPCGETB(in));
		else if (!(mask&8))
			err |= (int)PPCGETB(in);
		*(s-1) = '\0';
		if (err)
			ill(dp,in);
	}

	static void fmr(struct DisasmPara_PPC *dp,ppc_word in)
	{
		sprintf(dp->opcode, "fmr%s", rcsel[in&1]);
		sprintf(dp->operands, "f%d, f%d", (int)PPCGETD(in), (int)PPCGETB(in));
	}

	static void fdab(struct DisasmPara_PPC *dp,ppc_word in,const char *name,int mask)
		/* indexed float instruction: xxxx fD,rA,rB */
	{
		strcpy(dp->opcode,name);
		fd_ra_rb(dp->operands,in,mask);
	}


	static void fcmp(struct DisasmPara_PPC *dp,ppc_word in,char c)
	{
		if (in & 0x00600001) {
			ill(dp,in);
		}
		else {
			sprintf(dp->opcode,"fcmp%c",c);
			sprintf(dp->operands,"cr%d,f%d,f%d",(int)PPCGETCRD(in),
				(int)PPCGETA(in),(int)PPCGETB(in));
		}
	}


	static void mtfsb(struct DisasmPara_PPC *dp,ppc_word in,int n)
	{
		if (in & (PPCAMASK|PPCBMASK)) {
			ill(dp,in);
		}
		else {
			sprintf(dp->opcode,"mtfsb%d%s",n,rcsel[in&1]);
			sprintf(dp->operands,"%d",(int)PPCGETD(in));
		}
	}


	//////////////////////////////////////////////////////////////////////////////////
	//PAIRED
	//////////////////////////////////////////////////////////////////////////////////

	/*
	sprintf(buf, "psq_lx", FD);
	sprintf(buf, "psq_stx", FD);
	sprintf(buf, "psq_lux", FD);
	sprintf(buf, "psq_stux", FD);
	*/
#define RA ((inst >> 16) & 0x1f)
#define RB ((inst >> 11) & 0x1f)
#define RC ((inst >> 6) & 0x1f)
#define RD ((inst >> 21) & 0x1f)
#define RS ((inst >> 21) & 0x1f)
#define FA ((inst >> 16) & 0x1f)
#define FB ((inst >> 11) & 0x1f)
#define FC ((inst >> 6) & 0x1f)
#define FD ((inst >> 21) & 0x1f)
#define FS ((inst >> 21) & 0x1f)
#define IMM (inst & 0xffff)
#define UIMM (inst & 0xffff)
#define OFS (inst & 0xffff)
#define OPCD ((inst >> 26) & 0x3f)
#define XO_10 ((inst >> 1) & 0x3ff)
#define XO_9 ((inst >> 1) & 0x1ff)
#define XO_5 ((inst >> 1) & 0x1f)
#define Rc (inst & 1)
#define SH ((inst >> 11) & 0x1f)
#define MB ((inst >> 6) & 0x1f)
#define ME ((inst >> 1) & 0x1f)
#define OE ((inst >> 10) & 1)
#define TO ((inst >> 21) & 0x1f)
#define CRFD ((inst >> 23) & 0x7)
#define CRFS ((inst >> 18) & 0x7)
#define CRBD ((inst >> 21) & 0x1f)
#define CRBA ((inst >> 16) & 0x1f)
#define CRBB ((inst >> 11) & 0x1f)
#define L ((inst >> 21) & 1)
#define NB ((inst >> 11) & 0x1f)
#define AA ((inst >> 1) & 1)
#define LK (inst & 1)
#define LI ((inst >> 2) & 0xffffff)
#define BO ((inst >> 21) & 0x1f)
#define BI ((inst >> 16) & 0x1f)
#define BD ((inst >> 2) & 0x3fff)

#define MTFSFI_IMM ((inst >> 12) & 0xf)
#define FM ((inst >> 17) & 0xff)
#define SR ((inst >> 16) & 0xf)
#define SPR ((inst >> 11) & 0x3ff)
#define TBR ((inst >> 11) & 0x3ff)
#define CRM ((inst >> 12) & 0xff)

	inline int SEX12(unsigned int x)
	{
		return x & 0x800 ? (x|0xFFFFF000) : x;
	} 

	static void ps(struct DisasmPara_PPC *dp,ppc_word inst)
	{
		char *op = dp->opcode;
		char *pr = dp->operands;
		switch ((inst>>1)&0x1F) 
		{
		case 6:
			strcpy(op, "ps_lux");
			sprintf(pr, "p%u, (r%u + r%u)", FD, RA, RB);
			return;

		case 18:
			strcpy(op, "ps_div");
			sprintf(pr, "p%u, p%u/p%u", FD, FA, FB);
			return;
		case 20:
			strcpy(op, "ps_sub");
			sprintf(pr, "p%u, p%u-p%u", FD, FA, FB);
			return;
		case 21:
			strcpy(op, "ps_add");
			sprintf(pr, "p%u, p%u+p%u", FD, FA, FB);
			return;
		case 23:
			strcpy(op, "ps_sel");
			sprintf(pr, "p%u>=0?p%u:p%u", FD, FA, FC);
			return;
		case 24:
			strcpy(op, "ps_res");
			sprintf(pr, "p%u, (1/p%u)", FD, FB);
			return;

		case 25:
			strcpy(op, "ps_mul");
			sprintf(pr, "p%u, p%u*p%u", FD, FA, FC);
			return;

		case 26: //rsqrte
			strcpy(op, "ps_rsqrte");
			sprintf(pr, "p%u, p%u", FD, FB);
			return;
		case 28: //msub
			strcpy(op, "ps_msub");
			sprintf(pr, "p%u, p%u*p%u-p%u", FD, FA, FC, FB);
			return;
		case 29: //madd
			strcpy(op, "ps_madd");
			sprintf(pr, "p%u, p%u*p%u+p%u", FD, FA, FC, FB);
			return;
		case 30: //nmsub
			strcpy(op, "ps_nmsub");
			sprintf(pr, "p%u, -(p%u*p%u-p%u)", FD, FA, FC, FB);
			return;
		case 31: //nmadd
			strcpy(op, "ps_nmadd");
			sprintf(pr, "p%u, -(p%u*p%u+p%u)", FD, FA, FC, FB);
			return;
		case 10:
			strcpy(op, "ps_sum0");
			sprintf(pr, "p%u, 0=p%u+p%u, 1=p%u", FD, FA, FB, FC);
			return;
		case 11:
			strcpy(op, "ps_sum1");
			sprintf(pr, "p%u, 0=p%u, 1=p%u+p%u", FD, FC, FA, FB);
			return;
		case 12:
			strcpy(op, "ps_muls0");
			sprintf(pr, "p%u, p%u*p%u[0]", FD, FA, FC);
			return;
		case 13:
			strcpy(op, "ps_muls1");
			sprintf(pr, "p%u, p%u*p%u[1]", FD, FA, FC);
			return;
		case 14:
			strcpy(op, "ps_madds0");
			sprintf(pr, "p%u, p%u*p%u[0]+p%u", FD, FA, FC, FB);
			return;
		case 15:
			strcpy(op, "ps_madds1");
			sprintf(pr, "p%u, p%u*p%u[1]+p%u", FD, FA, FC, FB);
			return;
		}

		switch ((inst>>1)&0x3FF) 
		{
			//10-bit suckers  (?)
		case 40: //nmadd
			strcpy(op, "ps_neg");
			sprintf(pr, "p%u, -p%u", FD, FB);
			return;
		case 72: //nmadd
			strcpy(op, "ps_mr");
			sprintf(pr, "p%u, p%u", FD, FB);
			return;
		case 136:
			strcpy(op, "ps_nabs");
			sprintf(pr, "p%u, -|p%u|", FD, FB);
			return;
		case 264:
			strcpy(op, "ps_abs");
			sprintf(pr, "p%u, |p%u|", FD, FB);
			return;
		case 0:
			strcpy(op, "ps_cmpu0");
			sprintf(pr, "ps_cmpu0");
			return;
		case 32:
			strcpy(op,"ps_cmpq0");
			sprintf(pr, "ps_cmpo0");
			return;
		case 64:
			strcpy(op,"ps_cmpu1");
			sprintf(pr, "ps_cmpu1");
			return;
		case 96:
			strcpy(op,"ps_cmpo1");
			sprintf(pr, "ps_cmpo1");
			return;
		case 528:
			strcpy(op,"ps_merge00");
			sprintf(pr, "p%u, p%u[0],p%u[0]", FD, FA, FB);
			return;
		case 560:
			strcpy(op,"ps_merge01");
			sprintf(pr, "p%u, p%u[0],p%u[1]", FD, FA, FB);
			return;
		case 592:
			strcpy(op,"ps_merge10");
			sprintf(pr, "p%u, p%u[1],p%u[0]", FD, FA, FB);
			return;
		case 624:
			strcpy(op,"ps_merge11");
			sprintf(pr, "p%u, p%u[1],p%u[1]", FD, FA, FB);
			return;
		case 1014:
			strcpy(op,"dcbz_l");
			*pr = '\0';
			return;
		}

		//	default:
		sprintf(op, "ps_%i",((inst>>1)&0x1f));
		strcpy(pr,"---");
		return;
	}

	static void ps_mem(struct DisasmPara_PPC *dp,ppc_word inst)
	{
		char *op = dp->opcode;
		char *pr = dp->operands;
		switch (PPCGETIDX(inst)) 
		{
		case 56:
			strcpy(op,"psq_l");
			sprintf(pr, "p%u, %i(r%u)", RS, SEX12(inst&0xFFF), RA);
			break;
		case 57:
			strcpy(op,"psq_lu");
			*pr = '\0';
			break;
		case 60:
			strcpy(op,"psq_st");
			sprintf(pr, "%i(r%u), p%u", SEX12(inst&0xFFF), RA, RS);
			break;
		case 61:
			strcpy(op,"psq_stu");
			sprintf(pr, "r%u, p%u ?", RA, RS);
			break;
		}
	}


	ppc_word *PPC_Disassemble(struct DisasmPara_PPC *dp)
		/* Disassemble PPC instruction and return a pointer to the next */
		/* instruction, or NULL if an error occured. */
	{
		ppc_word in = *(dp->instr);
		if (!dp->opcode || !dp->operands)
			return NULL;  /* no buffers */

	#if LITTLEENDIAN
		in = (in & 0xff)<<24 | (in & 0xff00)<<8 | (in & 0xff0000)>>8 |
			(in & 0xff000000)>>24;
	#endif
		dp->type = PPCINSTR_OTHER;
		dp->flags = 0;
		*(dp->operands) = 0;

		switch (PPCGETIDX(in)) 
		{
		case 0:
			{
				int block = in & 0x3FFFFFF;
				if (block) {
					sprintf(dp->opcode, "JITblock");
					sprintf(dp->operands, "%i", block);
				} else {
					strcpy(dp->opcode, "");
					strcpy(dp->operands, "---");
				}
			}
			break;
		case 1:
			sprintf(dp->opcode,"HLE");
			//HLE call
			break;
		case 2:
			trapi(dp,in,PPCF_64);  /* tdi */
			break;

		case 3:
			trapi(dp,in,0);  /* twi */
			break;
		case 4:
			ps(dp,in);
			break;
		case 56:
		case 57:
		case 60:
		case 61:
			ps_mem(dp,in);
			break;


		case 7: 
			strcpy(dp->opcode,"mulli");
			imm(dp,in,0,0,0);
			break;

		case 8:
			strcpy(dp->opcode,"subfic");
			imm(dp,in,0,0,0);
			break;

		case 10:
			cmpi(dp,in,1);  /* cmpli */
			break;

		case 11:
			cmpi(dp,in,0);  /* cmpi */
			break;

		case 12:
			addi(dp,in,"ic");  /* addic */
			break;

		case 13:
			addi(dp,in,"ic.");  /* addic. */
			break;

		case 14:
			addi(dp,in,"i");  /* addi */
			break;

		case 15:
			addi(dp,in,"is");  /* addis */
			break;

		case 16:
			bc(dp,in);
			break;

		case 17:
			if ((in & ~PPCIDXMASK) == 2)
				strcpy(dp->opcode,"sc");
			else
				ill(dp,in);
			break;

		case 18:
			bli(dp,in);
			break;

		case 19:
			switch (PPCGETIDX2(in)) {
			case 0:
				mcrf(dp,in,'\0');  /* mcrf */
				break;

			case 16:
				branch(dp,in,"lr",0,0);  /* bclr */
				break;

			case 33:
				crop(dp,in,"nor","not");  /* crnor */
				break;

			case 50:
				nooper(dp,in,"rfi",PPCF_SUPER);
				break;

			case 129:
				crop(dp,in,"andc",NULL);  /* crandc */
				break;

			case 150:
				nooper(dp,in,"isync",0);
				break;

			case 193:
				crop(dp,in,"xor","clr");  /* crxor */
				break;

			case 225:
				crop(dp,in,"nand",NULL);  /* crnand */
				break;

			case 257:
				crop(dp,in,"and",NULL);  /* crand */
				break;

			case 289:
				crop(dp,in,"eqv","set");  /* creqv */
				break;

			case 417:
				crop(dp,in,"orc",NULL);  /* crorc */
				break;

			case 449:
				crop(dp,in,"or","move");  /* cror */
				break;

			case 528:
				branch(dp,in,"ctr",0,0);  /* bcctr */
				break;

			default:
				ill(dp,in);
				break;
			}
			break;

		case 20:
			rlw(dp,in,"imi",0);  /* rlwimi */
			break;

		case 21:
			rlw(dp,in,"inm",0);  /* rlwinm */
			break;

		case 23:
			rlw(dp,in,"nm",1);  /* rlwnm */
			break;

		case 24:
			if (in & ~PPCIDXMASK)
				ori(dp,in,"ori");
			else
				strcpy(dp->opcode,"nop");
			break;

		case 25:
			ori(dp,in,"oris");
			break;

		case 26:
			ori(dp,in,"xori");
			break;

		case 27:
			ori(dp,in,"xoris");
			break;

		case 28:
			ori(dp,in,"andi.");
			break;

		case 29:
			ori(dp,in,"andis.");
			break;

		case 30:
			switch (in & 0x1c) {
			case 0:
				rld(dp,in,"icl",0);  /* rldicl */
				break;
			case 1:
				rld(dp,in,"icr",0);  /* rldicr */
				break;
			case 2:
				rld(dp,in,"ic",0);   /* rldic */
				break;
			case 3:
				rld(dp,in,"imi",0);  /* rldimi */
				break;
			case 4:
				rld(dp,in,in&2?"cl":"cr",1);  /* rldcl, rldcr */
				break;
			default:
				ill(dp,in);
				break;
			}
			break;

		case 31:
			switch (PPCGETIDX2(in)) {
			case 0:
			case 32:
				if (in & 1)
					ill(dp,in);
				else
					cmp(dp,in);  /* cmp, cmpl */
				break;

			case 4:
				if (in & 1)
					ill(dp,in);
				else
					trap(dp,in,0);  /* tw */
				break;

			case 8:
			case (PPCOE>>1)+8:
				dab(dp,swapab(in),"subc",7,0,1,-1,0);
				break;

			case 9:
				dab(dp,in,"mulhdu",7,0,0,-1,PPCF_64);
				break;

			case 10:
			case (PPCOE>>1)+10:
				dab(dp,in,"addc",7,0,1,-1,0);
				break;

			case 11:
				dab(dp,in,"mulhwu",7,0,0,-1,0);
				break;

			case 19:
				if (in & (PPCAMASK|PPCBMASK))
					ill(dp,in);
				else
					dab(dp,in,"mfcr",4,0,0,0,0);
				break;

			case 20:
				dab(dp,in,"lwarx",7,0,0,0,0);
				break;

			case 21:
				dab(dp,in,"ldx",7,0,0,0,PPCF_64);
				break;

			case 23:
				dab(dp,in,"lwzx",7,0,0,0,0);
				break;

			case 24:
				dab(dp,in,"slw",7,1,0,-1,0);
				break;

			case 26:
				if (in & PPCBMASK)
					ill(dp,in);
				else
					dab(dp,in,"cntlzw",6,1,0,-1,0);
				break;

			case 27:
				dab(dp,in,"sld",7,1,0,-1,PPCF_64);
				break;

			case 28:
				dab(dp,in,"and",7,1,0,-1,0);
				break;

			case 40:
			case (PPCOE>>1)+40:
				dab(dp,swapab(in),"sub",7,0,1,-1,0);
				break;

			case 53:
				dab(dp,in,"ldux",7,0,0,0,PPCF_64);
				break;

			case 54:
				if (in & PPCDMASK)
					ill(dp,in);
				else
					dab(dp,in,"dcbst",3,0,0,0,0);
				break;

			case 55:
				dab(dp,in,"lwzux",7,0,0,0,0);
				break;

			case 58:
				if (in & PPCBMASK)
					ill(dp,in);
				else
					dab(dp,in,"cntlzd",6,1,0,-1,PPCF_64);
				break;

			case 60:
				dab(dp,in,"andc",7,1,0,-1,0);
				break;

			case 68:
				trap(dp,in,PPCF_64);  /* td */
				break;

			case 73:
				dab(dp,in,"mulhd",7,0,0,-1,PPCF_64);
				break;

			case 75:
				dab(dp,in,"mulhw",7,0,0,-1,0);
				break;

			case 83:
				if (in & (PPCAMASK|PPCBMASK))
					ill(dp,in);
				else
					dab(dp,in,"mfmsr",4,0,0,0,PPCF_SUPER);
				break;

			case 84:
				dab(dp,in,"ldarx",7,0,0,0,PPCF_64);
				break;

			case 86:
				if (in & PPCDMASK)
					ill(dp,in);
				else
					dab(dp,in,"dcbf",3,0,0,0,0);
				break;

			case 87:
				dab(dp,in,"lbzx",7,0,0,0,0);
				break;

			case 104:
			case (PPCOE>>1)+104:
				if (in & PPCBMASK)
					ill(dp,in);
				else
					dab(dp,in,"neg",6,0,1,-1,0);
				break;

			case 119:
				dab(dp,in,"lbzux",7,0,0,0,0);
				break;

			case 124:
				if (PPCGETD(in) == PPCGETB(in))
					dab(dp,in,"not",6,1,0,-1,0);
				else
					dab(dp,in,"nor",7,1,0,-1,0);
				break;

			case 136:
			case (PPCOE>>1)+136:
				dab(dp,in,"subfe",7,0,1,-1,0);
				break;

			case 138:
			case (PPCOE>>1)+138:
				dab(dp,in,"adde",7,0,1,-1,0);
				break;

			case 144:
				mtcr(dp,in);
				break;

			case 146:
				if (in & (PPCAMASK|PPCBMASK))
					ill(dp,in);
				else
					dab(dp,in,"mtmsr",4,0,0,0,PPCF_SUPER);
				break;

			case 149:
				dab(dp,in,"stdx",7,0,0,0,PPCF_64);
				break;

			case 150:
				dab(dp,in,"stwcx.",7,0,0,1,0);
				break;

			case 151:
				dab(dp,in,"stwx",7,0,0,0,0);
				break;

			case 181:
				dab(dp,in,"stdux",7,0,0,0,PPCF_64);
				break;

			case 183:
				dab(dp,in,"stwux",7,0,0,0,0);
				break;

			case 200:
			case (PPCOE>>1)+200:
				if (in & PPCBMASK)
					ill(dp,in);
				else
					dab(dp,in,"subfze",6,0,1,-1,0);
				break;

			case 202:
			case (PPCOE>>1)+202:
				if (in & PPCBMASK)
					ill(dp,in);
				else
					dab(dp,in,"addze",6,0,1,-1,0);
				break;

			case 210:
				msr(dp,in,1);  /* mfsr */
				break;

			case 214:
				dab(dp,in,"stdcx.",7,0,0,1,PPCF_64);
				break;

			case 215:
				dab(dp,in,"stbx",7,0,0,0,0);
				break;

			case 232:
			case (PPCOE>>1)+232:
				if (in & PPCBMASK)
					ill(dp,in);
				else
					dab(dp,in,"subfme",6,0,1,-1,0);
				break;

			case 233:
			case (PPCOE>>1)+233:
				dab(dp,in,"mulld",7,0,1,-1,PPCF_64);
				break;

			case 234:
			case (PPCOE>>1)+234:
				if (in & PPCBMASK)
					ill(dp,in);
				else
					dab(dp,in,"addme",6,0,1,-1,0);
				break;

			case 235:
			case (PPCOE>>1)+235:
				dab(dp,in,"mullw",7,0,1,-1,0);
				break;

			case 242:
				if (in & PPCAMASK)
					ill(dp,in);
				else
					dab(dp,in,"mtsrin",5,0,0,0,PPCF_SUPER);
				break;

			case 246:
				if (in & PPCDMASK)
					ill(dp,in);
				else
					dab(dp,in,"dcbtst",3,0,0,0,0);
				break;

			case 247:
				dab(dp,in,"stbux",7,0,0,0,0);
				break;

			case 266:
			case (PPCOE>>1)+266:
				dab(dp,in,"add",7,0,1,-1,0);
				break;

			case 278:
				if (in & PPCDMASK)
					ill(dp,in);
				else
					dab(dp,in,"dcbt",3,0,0,0,0);
				break;

			case 279:
				dab(dp,in,"lhzx",7,0,0,0,0);
				break;

			case 284:
				dab(dp,in,"eqv",7,1,0,-1,0);
				break;

			case 306:
				if (in & (PPCDMASK|PPCAMASK))
					ill(dp,in);
				else
					dab(dp,in,"tlbie",1,0,0,0,PPCF_SUPER);
				break;

			case 310:
				dab(dp,in,"eciwx",7,0,0,0,0);
				break;

			case 311:
				dab(dp,in,"lhzux",7,0,0,0,0);
				break;

			case 316:
				dab(dp,in,"xor",7,1,0,-1,0);
				break;

			case 339:
				mspr(dp,in,0);  /* mfspr */
				break;

			case 341:
				dab(dp,in,"lwax",7,0,0,0,PPCF_64);
				break;

			case 343:
				dab(dp,in,"lhax",7,0,0,0,0);
				break;

			case 370:
				nooper(dp,in,"tlbia",PPCF_SUPER);
				break;

			case 371:
				mtb(dp,in);  /* mftb */
				break;

			case 373:
				dab(dp,in,"lwaux",7,0,0,0,PPCF_64);
				break;

			case 375:
				dab(dp,in,"lhaux",7,0,0,0,0);
				break;

			case 407:
				dab(dp,in,"sthx",7,0,0,0,0);
				break;

			case 412:
				dab(dp,in,"orc",7,1,0,-1,0);
				break;

			case 413:
				sradi(dp,in);  /* sradi */
				break;

			case 434:
				if (in & (PPCDMASK|PPCAMASK))
					ill(dp,in);
				else
					dab(dp,in,"slbie",1,0,0,0,PPCF_SUPER|PPCF_64);
				break;

			case 438:
				dab(dp,in,"ecowx",7,0,0,0,0);
				break;

			case 439:
				dab(dp,in,"sthux",7,0,0,0,0);
				break;

			case 444:
				if (PPCGETD(in) == PPCGETB(in))
					dab(dp,in,"mr",6,1,0,-1,0);
				else
					dab(dp,in,"or",7,1,0,-1,0);
				break;

			case 457:
			case (PPCOE>>1)+457:
				dab(dp,in,"divdu",7,0,1,-1,PPCF_64);
				break;

			case 459:
			case (PPCOE>>1)+459:
				dab(dp,in,"divwu",7,0,1,-1,0);
				break;

			case 467:
				mspr(dp,in,1);  /* mtspr */
				break;

			case 470:
				if (in & PPCDMASK)
					ill(dp,in);
				else
					dab(dp,in,"dcbi",3,0,0,0,0);
				break;

			case 476:
				dab(dp,in,"nand",7,1,0,-1,0);
				break;

			case 489:
			case (PPCOE>>1)+489:
				dab(dp,in,"divd",7,0,1,-1,PPCF_64);
				break;

			case 491:
			case (PPCOE>>1)+491:
				dab(dp,in,"divw",7,0,1,-1,0);
				break;

			case 498:
				nooper(dp,in,"slbia",PPCF_SUPER|PPCF_64);
				break;

			case 512:
				if (in & 0x007ff801)
					ill(dp,in);
				else {
					strcpy(dp->opcode,"mcrxr");
					sprintf(dp->operands,"cr%d",(int)PPCGETCRD(in));
				}
				break;

			case 533:
				dab(dp,in,"lswx",7,0,0,0,0);
				break;

			case 534:
				dab(dp,in,"lwbrx",7,0,0,0,0);
				break;

			case 535:
				fdab(dp,in,"lfsx",7);
				break;

			case 536:
				dab(dp,in,"srw",7,1,0,-1,0);
				break;

			case 539:
				dab(dp,in,"srd",7,1,0,-1,PPCF_64);
				break;

			case 566:
				nooper(dp,in,"tlbsync",PPCF_SUPER);
				break;

			case 567:
				fdab(dp,in,"lfsux",7);
				break;

			case 595:
				msr(dp,in,0);  /* mfsr */
				break;

			case 597:
				rrn(dp,in,"lswi",0,0,0,0);
				break;

			case 598:
				nooper(dp,in,"sync",PPCF_SUPER);
				break;

			case 599:
				fdab(dp,in,"lfdx",7);
				break;

			case 631:
				fdab(dp,in,"lfdux",7);
				break;

			case 659:
				if (in & PPCAMASK)
					ill(dp,in);
				else
					dab(dp,in,"mfsrin",5,0,0,0,PPCF_SUPER);
				break;

			case 661:
				dab(dp,in,"stswx",7,0,0,0,0);
				break;

			case 662:
				dab(dp,in,"stwbrx",7,0,0,0,0);
				break;

			case 663:
				fdab(dp,in,"stfsx",7);
				break;

			case 695:
				fdab(dp,in,"stfsux",7);
				break;

			case 725:
				rrn(dp,in,"stswi",0,0,0,0);
				break;

			case 727:
				fdab(dp,in,"stfdx",7);
				break;

			case 759:
				fdab(dp,in,"stfdux",7);
				break;

			case 790:
				dab(dp,in,"lhbrx",7,0,0,0,0);
				break;

			case 792:
				dab(dp,in,"sraw",7,1,0,-1,0);
				break;

			case 794:
				dab(dp,in,"srad",7,1,0,-1,PPCF_64);
				break;

			case 824:
				rrn(dp,in,"srawi",1,0,-1,0);
				break;

			case 854:
				nooper(dp,in,"eieio",PPCF_SUPER);
				break;

			case 918:
				dab(dp,in,"sthbrx",7,0,0,0,0);
				break;

			case 922:
				if (in & PPCBMASK)
					ill(dp,in);
				else
					dab(dp,in,"extsh",6,1,0,-1,0);
				break;

			case 954:
				if (in & PPCBMASK)
					ill(dp,in);
				else
					dab(dp,in,"extsb",6,1,0,-1,0);
				break;

			case 982:
				if (in & PPCDMASK)
					ill(dp,in);
				else
					dab(dp,in,"icbi",3,0,0,0,0);
				break;

			case 983:
				fdab(dp,in,"stfiwx",7);
				break;

			case 986:
				if (in & PPCBMASK)
					ill(dp,in);
				else
					dab(dp,in,"extsw",6,1,0,-1,PPCF_64);
				break;

			case 1014:
				if (in & PPCDMASK)
					ill(dp,in);
				else
					dab(dp,in,"dcbz",3,0,0,0,0);
				break;

			default:
				ill(dp,in);
				break;
			}
			break;

		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
		case 38:
		case 39:
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			ldst(dp,in,ldstnames[PPCGETIDX(in)-32],'r',0);
			break;

		case 48:
		case 49:
		case 50:
		case 51:
		case 52:
		case 53:
		case 54:
		case 55:
			ldst(dp,in,ldstnames[PPCGETIDX(in)-32],'f',0);
			break;

		case 58:
			switch (in & 3) {
			case 0:
				ldst(dp,in&~3,"ld",'r',PPCF_64);
				break;
			case 1:
				ldst(dp,in&~3,"ldu",'r',PPCF_64);
				break;
			case 2:
				ldst(dp,in&~3,"lwa",'r',PPCF_64);
				break;
			default:
				ill(dp,in);
				break;
			}
			break;

		case 59:
			switch (in & 0x3e) {
			case 36:
				fdabc(dp,in,"divs",5,0);
				break;

			case 40:
				fdabc(dp,in,"subs",5,0);
				break;

			case 42:
				fdabc(dp,in,"adds",5,0);
				break;

			case 44:
				fdabc(dp,in,"sqrts",2,0);
				break; 

			case 48:
				fdabc(dp,in,"res",2,0);
				break;

			case 50:
				fdabc(dp,in,"muls",6,0);
				break;

			case 56:
				fdabc(dp,in,"msubs",7,0);
				break;

			case 58:
				fdabc(dp,in,"madds",7,0);
				break;

			case 60:
				fdabc(dp,in,"nmsubs",7,0);
				break;

			case 62:
				fdabc(dp,in,"nmadds",7,0);
				break;

			default:
				ill(dp,in);
				break;
			}
			break;

		case 62:
			switch (in & 3) {
			case 0:
				ldst(dp,in&~3,"std",'r',PPCF_64);
				break;
			case 1:
				ldst(dp,in&~3,"stdu",'r',PPCF_64);
				break;
			default:
				ill(dp,in);
				break;
			}
			break;

		case 63:
			if (in & 32) {
				switch (in & 0x1e) {
				case 4:
					fdabc(dp,in,"div",5,0);
					break;

				case 8:
					fdabc(dp,in,"sub",5,0);
					break;

				case 10:
					fdabc(dp,in,"add",5,0);
					break;

				case 12:
					fdabc(dp,in,"sqrt",2,0);
					break;

				case 14:
					fdabc(dp,in,"sel",7,0);
					break;

				case 18:
					fdabc(dp,in,"mul",6,0);
					break;

				case 20:
					fdabc(dp,in,"rsqrte",1,0);
					break;

				case 24:
					fdabc(dp,in,"msub",7,0);
					break;

				case 26:
					fdabc(dp,in,"madd",7,0);
					break;

				case 28:
					fdabc(dp,in,"nmsub",7,0);
					break;

				case 30:
					fdabc(dp,in,"nmadd",7,0);
					break;

				case 52:
					sprintf(dp->opcode, "XXX dp 52");
					break;

				default:
					ill(dp,in);
					break;
				}
			}
			else {
				switch (PPCGETIDX2(in)) {
				case 0:
					fcmp(dp,in,'u');
					break;

				case 12:
					fdabc(dp,in,"rsp",1,0);  // 10
					break;

				case 14:
					fdabc(dp,in,"ctiw",1,0);  // 10
					break;

				case 15:
					fdabc(dp,in,"ctiwz",1,0); // 10
					break;

				case 32:
					fcmp(dp,in,'o');
					break;

				case 38:
					mtfsb(dp,in,1);
					break;

				case 40:
					fdabc(dp,in,"neg",10,0);
					break;

				case 64:
					mcrf(dp,in,'s');  /* mcrfs */
					break;

				case 70:
					mtfsb(dp,in,0);
					break;

				case 72:
					fmr(dp,in);
					break;

				case 134:
					if (!(in & 0x006f0800)) {
						sprintf(dp->opcode,"mtfsfi%s",rcsel[in&1]);
						sprintf(dp->operands,"cr%d,%d",(int)PPCGETCRD(in),
							(int)(in & 0xf000)>>12);
					}
					else
						ill(dp,in);
					break;

				case 136:
					fdabc(dp,in,"nabs",10,0);
					break;

				case 264:
					fdabc(dp,in,"abs",10,0);
					break;

				case 583:
					if (in & (PPCAMASK|PPCBMASK))
						ill(dp,in);
					else
						dab(dp,in,"mffs",4,0,0,-1,0);
					break;

				case 711:
					if (!(in & 0x02010000)) {
						sprintf(dp->opcode,"mtfsf%s",rcsel[in&1]);
						sprintf(dp->operands,"0x%x,%d",
							(unsigned)(in & 0x01fe)>>17,(int)PPCGETB(in));
					}
					else
						ill(dp,in);
					break;

				case 814:
					fdabc(dp,in,"fctid",10,PPCF_64);
					break;

				case 815:
					fdabc(dp,in,"fctidz",10,PPCF_64);
					break;

				case 846:
					fdabc(dp,in,"fcfid",10,PPCF_64);
					break;

				default:
					ill(dp,in);
					break;
				}
			}
			break;

		default:
			ill(dp,in);
			break;
		}
		return (dp->instr + 1);
	}

}  // namespace

// What were MS thinking?
#ifdef _WIN32
#define snprintf _snprintf
#endif

// simplified interface
void DisassembleGekko(unsigned int opcode, unsigned int curInstAddr, char *dest, int max_size)
{
	char opcodeStr[64], operandStr[64];
	PPCDisasm::DisasmPara_PPC dp;
	unsigned int opc, adr;

	opc = opcode;
	adr = curInstAddr;

	dp.opcode = opcodeStr;
	dp.operands = operandStr;
	dp.instr = (PPCDisasm::ppc_word *)&opc;
	dp.iaddr = (PPCDisasm::ppc_word *)&adr;

	PPCDisasm::PPC_Disassemble(&dp);

	snprintf(dest, max_size, "%s\t%s", opcodeStr, operandStr);
}


static const char *gprnames[] = 
{
    " r0", " r1", " r2", " r3", " r4", " r5", " r6", " r7",
    " r8", " r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
};

const char *GetGPRName(unsigned int index)
{
    if (index < 32)
        return gprnames[index];
    return 0;
}


static const char *fprnames[] = 
{
    " f0", " f1", " f2", " f3", " f4", " f5", " f6", " f7",
    " f8", " f9", "f10", "f11", "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31"
};

const char *GetFPRName(unsigned int index)
{
    if (index < 32)
        return fprnames[index];
    return 0;
}
