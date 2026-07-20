/////////////////////////////////////////////////////////////////////////
// $Id: dis_tables.h 11878 2013-10-11 20:09:51Z sshwarts $
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2005-2012 Stanislav Shwartsman
//          Written by Stanislav Shwartsman [sshwarts at sourceforge net]
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

#ifndef _BX_DISASM_TABLES_
#define _BX_DISASM_TABLES_

// opcode table attributes
#define _GROUPN        1
#define _SPLIT11B      2
#define _GRPFP         3
#define _GRP3DNOW      4
#define _GRPSSE        5
#define _GRPSSE66      6
#define _GRPSSEF2      7
#define _GRPSSEF3      8
#define _GRPSSENONE    9
#define _GRPSSE2       10
#define _GRPRM         11
#define _GRP3BOP       12
#define _GRP64B        13
#define _GRPVEXW       14

/* ************************************************************************ */
#define GRPSSE(n)       _GRPSSE,   BxDisasmGroupSSE_##n
#define GRPSSE2(n)      _GRPSSE2,  BxDisasmGroupSSE_##n
#define GRPAVX(n)       _GRPSSE,   BxDisasmGroupAVX_##n
#define GRPAVX2(n)      _GRPSSE2,  BxDisasmGroupAVX_##n
#define GRPN(n)         _GROUPN,   BxDisasmGroup##n
#define GRPRM(n)        _GRPRM,    BxDisasmGroupRm##n
#define GRPMOD(n)       _SPLIT11B, BxDisasmGroupMod##n
#define GRPFP(n)        _GRPFP,    BxDisasmFPGroup##n
#define GRP3DNOW        _GRP3DNOW, BxDisasm3DNowGroup
#define GR3BTAB(n)      _GRP3BOP,  BxDisasm3ByteOpTable##n
#define GR64BIT(n)      _GRP64B,   BxDisasmGrpOs64B_##n
#define GRPVEXW(n)      _GRPVEXW,  BxDisasmGrpVexW_##n
/* ************************************************************************ */

/* ************************************************************************ */
#define GRPSSE66(n)     _GRPSSE66,   &n
#define GRPSSEF2(n)     _GRPSSEF2,   &n
#define GRPSSEF3(n)     _GRPSSEF3,   &n
#define GRPSSENONE(n)   _GRPSSENONE, &n
/* ************************************************************************ */

#define Apw &disassembler::Apw
#define Apd &disassembler::Apd

#define AL_Reg &disassembler::AL_Reg
#define CL_Reg &disassembler::CL_Reg
#define AX_Reg &disassembler::AX_Reg
#define DX_Reg &disassembler::DX_Reg

#define EAX_Reg &disassembler::EAX_Reg
#define RAX_Reg &disassembler::RAX_Reg
#define RCX_Reg &disassembler::RCX_Reg

#define CS &disassembler::CS
#define DS &disassembler::DS
#define ES &disassembler::ES
#define SS &disassembler::SS
#define FS &disassembler::FS
#define GS &disassembler::GS

#define Sw &disassembler::Sw

#define Cd &disassembler::Cd
#define Cq &disassembler::Cq

#define Dd &disassembler::Dd
#define Dq &disassembler::Dq

#define Reg8 &disassembler::Reg8
#define   RX &disassembler::RX
#define  ERX &disassembler::ERX
#define  RRX &disassembler::RRX

#define Eb  &disassembler::Eb
#define Ew  &disassembler::Ew
#define Ed  &disassembler::Ed
#define Eq  &disassembler::Eq
#define Ey  &disassembler::Ey
#define Ebd &disassembler::Ebd
#define Ewd &disassembler::Ewd

#define Gb &disassembler::Gb
#define Gw &disassembler::Gw
#define Gd &disassembler::Gd
#define Gq &disassembler::Gq
#define Gy &disassembler::Gy

#define By &disassembler::By

#define I1 &disassembler::I1
#define Ib &disassembler::Ib
#define Iw &disassembler::Iw
#define Id &disassembler::Id
#define Iq &disassembler::Iq

#define IbIb &disassembler::IbIb
#define IwIb &disassembler::IwIb

#define sIbw &disassembler::sIbw
#define sIbd &disassembler::sIbd
#define sIbq &disassembler::sIbq
#define sIdq &disassembler::sIdq

#define ST0 &disassembler::ST0
#define STi &disassembler::STi

#define Rw &disassembler::Rw
#define Rd &disassembler::Rd
#define Rq &disassembler::Rq
#define Ry &disassembler::Ry

#define Pq &disassembler::Pq
#define Qd &disassembler::Qd
#define Qq &disassembler::Qq
#define Nq &disassembler::Nq

#define  Vq &disassembler::Vq
#define Vdq &disassembler::Vdq
#define Vss &disassembler::Vss
#define Vsd &disassembler::Vsd
#define Vps &disassembler::Vps
#define Vpd &disassembler::Vpd
#define VIb &disassembler::VIb

#define Ups &disassembler::Ups
#define Upd &disassembler::Upd
#define Udq &disassembler::Udq
#define Uq  &disassembler::Uq

#define  Wb &disassembler::Wb
#define  Ww &disassembler::Ww
#define  Wd &disassembler::Wd
#define  Wq &disassembler::Wq
#define Wdq &disassembler::Wdq
#define Wss &disassembler::Wss
#define Wsd &disassembler::Wsd
#define Wps &disassembler::Wps
#define Wpd &disassembler::Wpd

#define Hdq &disassembler::Hdq
#define Hps &disassembler::Hps
#define Hpd &disassembler::Hpd
#define Hss &disassembler::Hss
#define Hsd &disassembler::Hsd

#define Ob &disassembler::Ob
#define Ow &disassembler::Ow
#define Od &disassembler::Od
#define Oq &disassembler::Oq

#define  Ma &disassembler::Ma
#define  Mp &disassembler::Mp
#define  Ms &disassembler::Ms
#define  Mx &disassembler::Mx
#define  Mb &disassembler::Mb
#define  Mw &disassembler::Mw
#define  Md &disassembler::Md
#define  Mq &disassembler::Mq
#define  Mt &disassembler::Mt
#define Mdq &disassembler::Mdq
#define Mps &disassembler::Mps
#define Mpd &disassembler::Mpd
#define Mss &disassembler::Mss
#define Msd &disassembler::Msd

#define VSib &disassembler::VSib

#define Xb &disassembler::Xb
#define Xw &disassembler::Xw
#define Xd &disassembler::Xd
#define Xq &disassembler::Xq

#define Yb &disassembler::Yb
#define Yw &disassembler::Yw
#define Yd &disassembler::Yd
#define Yq &disassembler::Yq

#define sYq  &disassembler::sYq
#define sYdq &disassembler::sYdq

#define Jb &disassembler::Jb
#define Jw &disassembler::Jw
#define Jd &disassembler::Jd

#define XX 0

const struct BxDisasmOpcodeInfo_t
#include "opcodes.inc"
#include "dis_tables_x87.inc"
#include "dis_tables_sse.inc"
#include "dis_tables_avx.inc"
#include "dis_tables_xop.inc"
#include "dis_tables.inc"

#undef XX

#endif
