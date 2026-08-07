/////////////////////////////////////////////////////////////////////////
// $Id: dis_tables_sse.inc 11878 2013-10-11 20:09:51Z sshwarts $
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2005-2013 Stanislav Shwartsman
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
//
/////////////////////////////////////////////////////////////////////////

/* ************************************************************************ */
/* SSE opcodes */

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_ERR[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f10[4] = {
  /* -- */ { 0, &Ia_movups_Vps_Wps },
  /* 66 */ { 0, &Ia_movupd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_movss_Vss_Wss },
  /* F2 */ { 0, &Ia_movsd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f11[4] = {
  /* -- */ { 0, &Ia_movups_Wps_Vps },
  /* 66 */ { 0, &Ia_movupd_Wpd_Vpd },
  /* F3 */ { 0, &Ia_movss_Wss_Vss },
  /* F2 */ { 0, &Ia_movsd_Wsd_Vsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupModMOVHLPS[2] = {
  /* R */ { 0, &Ia_movhlps_Vps_Udq },
  /* M */ { 0, &Ia_movlps_Vps_Mq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f12[4] = {
  /* -- */ { GRPMOD(MOVHLPS) },
  /* 66 */ { 0, &Ia_movlpd_Vpd_Mq },
  /* F3 */ { 0, &Ia_movsldup_Vdq_Wdq },
  /* F2 */ { 0, &Ia_movddup_Vdq_Wq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f13[2] = {
  /* -- */ { 0, &Ia_movlps_Mq_Vps },
  /* 66 */ { 0, &Ia_movlpd_Mq_Vpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f14[2] = {
  /* -- */ { 0, &Ia_unpcklps_Vps_Wps },
  /* 66 */ { 0, &Ia_unpcklpd_Vpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f15[2] = {
  /* -- */ { 0, &Ia_unpckhps_Vps_Wps },
  /* 66 */ { 0, &Ia_unpckhpd_Vpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupModMOVLHPS[2] = {
  /* R */ { 0, &Ia_movlhps_Vps_Udq },
  /* M */ { 0, &Ia_movhps_Vps_Mq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f16[4] = {
  /* -- */ { GRPMOD(MOVLHPS) },
  /* 66 */ { 0, &Ia_movhpd_Vpd_Mq },
  /* F3 */ { 0, &Ia_movshdup_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f17[2] = {
  /* -- */ { 0, &Ia_movhps_Mq_Vps },
  /* 66 */ { 0, &Ia_movhpd_Mq_Vpd },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f28[2] = {
  /* -- */ { 0, &Ia_movaps_Vps_Wps },
  /* 66 */ { 0, &Ia_movapd_Vpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f29[2] = {
  /* -- */ { 0, &Ia_movaps_Wps_Vps },
  /* 66 */ { 0, &Ia_movapd_Wpd_Vpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2a[4] = {
  /* -- */ { 0, &Ia_cvtpi2ps_Vps_Qq },
  /* 66 */ { 0, &Ia_cvtpi2pd_Vpd_Qq },
  /* F3 */ { 0, &Ia_cvtsi2ss_Vss_Ey },
  /* F2 */ { 0, &Ia_cvtsi2sd_Vsd_Ey }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2b[4] = {
  /* -- */ { 0, &Ia_movntps_Mps_Vps },
  /* 66 */ { 0, &Ia_movntpd_Mpd_Vpd },
  /* F3 */ { 0, &Ia_movntss_Mss_Vss }, // SSE4A
  /* F2 */ { 0, &Ia_movntsd_Msd_Vsd }  // SSE4A
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2c[4] = {
  /* -- */ { 0, &Ia_cvttps2pi_Pq_Wps },
  /* 66 */ { 0, &Ia_cvttpd2pi_Pq_Wpd },
  /* F3 */ { 0, &Ia_cvttss2si_Gy_Wss },
  /* F2 */ { 0, &Ia_cvttsd2si_Gy_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2d[4] = {
  /* -- */ { 0, &Ia_cvtps2pi_Pq_Wps },
  /* 66 */ { 0, &Ia_cvtpd2pi_Pq_Wpd },
  /* F3 */ { 0, &Ia_cvtss2si_Gy_Wss },
  /* F2 */ { 0, &Ia_cvtsd2si_Gy_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2e[2] = {
  /* -- */ { 0, &Ia_ucomiss_Vss_Wss },
  /* 66 */ { 0, &Ia_ucomisd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2f[2] = {
  /* -- */ { 0, &Ia_comiss_Vss_Wss },
  /* 66 */ { 0, &Ia_comisd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f50[2] = {
  /* -- */ { 0, &Ia_movmskps_Gd_Ups },
  /* 66 */ { 0, &Ia_movmskpd_Gd_Upd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f51[4] = {
  /* -- */ { 0, &Ia_sqrtps_Vps_Wps },
  /* 66 */ { 0, &Ia_sqrtpd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_sqrtss_Vss_Wss },
  /* F2 */ { 0, &Ia_sqrtsd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f52[4] = {
  /* -- */ { 0, &Ia_rsqrtps_Vps_Wps },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_rsqrtss_Vss_Wss },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f53[4] = {
  /* -- */ { 0, &Ia_rcpps_Vps_Wps },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_rcpss_Vss_Wss },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f54[2] = {
  /* -- */ { 0, &Ia_andps_Vps_Wps },
  /* 66 */ { 0, &Ia_andpd_Vpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f55[2] = {
  /* -- */ { 0, &Ia_andnps_Vps_Wps },
  /* 66 */ { 0, &Ia_andnpd_Vpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f56[2] = {
  /* -- */ { 0, &Ia_orps_Vps_Wps },
  /* 66 */ { 0, &Ia_orpd_Vpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f57[2] = {
  /* -- */ { 0, &Ia_xorps_Vps_Wps },
  /* 66 */ { 0, &Ia_xorpd_Vpd_Wpd },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f58[4] = {
  /* -- */ { 0, &Ia_addps_Vps_Wps },
  /* 66 */ { 0, &Ia_addpd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_addss_Vss_Wss },
  /* F2 */ { 0, &Ia_addsd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f59[4] = {
  /* -- */ { 0, &Ia_mulps_Vps_Wps },
  /* 66 */ { 0, &Ia_mulpd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_mulss_Vss_Wss },
  /* F2 */ { 0, &Ia_mulsd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5a[4] = {
  /* -- */ { 0, &Ia_cvtps2pd_Vpd_Wps },
  /* 66 */ { 0, &Ia_cvtpd2ps_Vps_Wpd },
  /* F3 */ { 0, &Ia_cvtss2sd_Vsd_Wss },
  /* F2 */ { 0, &Ia_cvtsd2ss_Vss_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5b[4] = {
  /* -- */ { 0, &Ia_cvtdq2ps_Vps_Wdq },
  /* 66 */ { 0, &Ia_cvtps2dq_Vdq_Wps },
  /* F3 */ { 0, &Ia_cvttps2dq_Vdq_Wps },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5c[4] = {
  /* -- */ { 0, &Ia_subps_Vps_Wps },
  /* 66 */ { 0, &Ia_subpd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_subss_Vss_Wss },
  /* F2 */ { 0, &Ia_subsd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5d[4] = {
  /* -- */ { 0, &Ia_minps_Vps_Wps },
  /* 66 */ { 0, &Ia_minpd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_minss_Vss_Wss },
  /* F2 */ { 0, &Ia_minsd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5e[4] = {
  /* -- */ { 0, &Ia_divps_Vps_Wps },
  /* 66 */ { 0, &Ia_divpd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_divss_Vss_Wss },
  /* F2 */ { 0, &Ia_divsd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5f[4] = {
  /* -- */ { 0, &Ia_maxps_Vps_Wps },
  /* 66 */ { 0, &Ia_maxpd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_maxss_Vss_Wss },
  /* F2 */ { 0, &Ia_maxsd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f60[2] = {
  /* -- */ { 0, &Ia_punpcklbw_Pq_Qd },
  /* 66 */ { 0, &Ia_punpcklbw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f61[2] = {
  /* -- */ { 0, &Ia_punpcklwd_Pq_Qd },
  /* 66 */ { 0, &Ia_punpcklwd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f62[2] = {
  /* -- */ { 0, &Ia_punpckldq_Pq_Qd },
  /* 66 */ { 0, &Ia_punpckldq_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f63[2] = {
  /* -- */ { 0, &Ia_packsswb_Pq_Qq },
  /* 66 */ { 0, &Ia_packsswb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f64[2] = {
  /* -- */ { 0, &Ia_pcmpgtb_Pq_Qq },
  /* 66 */ { 0, &Ia_pcmpgtb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f65[2] = {
  /* -- */ { 0, &Ia_pcmpgtw_Pq_Qq },
  /* 66 */ { 0, &Ia_pcmpgtw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f66[2] = {
  /* -- */ { 0, &Ia_pcmpgtd_Pq_Qq },
  /* 66 */ { 0, &Ia_pcmpgtd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f67[2] = {
  /* -- */ { 0, &Ia_packuswb_Pq_Qq },
  /* 66 */ { 0, &Ia_packuswb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f68[2] = {
  /* -- */ { 0, &Ia_punpckhbw_Pq_Qq },
  /* 66 */ { 0, &Ia_punpckhbw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f69[2] = {
  /* -- */ { 0, &Ia_punpckhwd_Pq_Qq },
  /* 66 */ { 0, &Ia_punpckhwd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6a[2] = {
  /* -- */ { 0, &Ia_punpckhdq_Pq_Qq },
  /* 66 */ { 0, &Ia_punpckhdq_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6b[2] = {
  /* -- */ { 0, &Ia_packssdw_Pq_Qq },
  /* 66 */ { 0, &Ia_packssdw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6e[2] = {
  /* -- */ { 0, &Ia_movd_Pq_Ed },
  /* 66 */ { 0, &Ia_movd_Vdq_Ed }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6eQ[2] = {
  /* -- */ { 0, &Ia_movq_Pq_Eq },
  /* 66 */ { 0, &Ia_movq_Vdq_Eq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6f[4] = {
  /* -- */ { 0, &Ia_movq_Pq_Qq },
  /* 66 */ { 0, &Ia_movdqa_Vdq_Wdq },
  /* F3 */ { 0, &Ia_movdqu_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f70[4] = {
  /* -- */ { 0, &Ia_pshufw_Pq_Qq_Ib },
  /* 66 */ { 0, &Ia_pshufd_Vdq_Wdq_Ib  },
  /* F3 */ { 0, &Ia_pshufhw_Vdq_Wdq_Ib },
  /* F2 */ { 0, &Ia_pshuflw_Vdq_Wdq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f74[2] = {
  /* -- */ { 0, &Ia_pcmpeqb_Pq_Qq },
  /* 66 */ { 0, &Ia_pcmpeqb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f75[2] = {
  /* -- */ { 0, &Ia_pcmpeqw_Pq_Qq },
  /* 66 */ { 0, &Ia_pcmpeqw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f76[2] = {
  /* -- */ { 0, &Ia_pcmpeqd_Pq_Qq },
  /* 66 */ { 0, &Ia_pcmpeqd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE4A_G17[8] = {
  /* 0 */ { 0, &Ia_extrq_Udq_Ib_Ib },       // SSE4A
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { 0, &Ia_Invalid },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { 0, &Ia_Invalid },
  /* 7 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f78[4] = {
  /* -- */ { 0, &Ia_vmread_Ed_Gd },         // VMX
  /* 66 */ { GRPN(SSE4A_G17) },             // SSE4A
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_insertq_Vdq_Uq_Ib_Ib }  // SSE4A
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f78Q[4] = {
  /* -- */ { 0, &Ia_vmread_Eq_Gq },         // VMX
  /* 66 */ { GRPN(SSE4A_G17) },             // SSE4A
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_insertq_Vdq_Uq_Ib_Ib }  // SSE4A
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f79[4] = {
  /* -- */ { 0, &Ia_vmwrite_Gd_Ed },        // VMX
  /* 66 */ { 0, &Ia_extrq_Vdq_Uq },         // SSE4A
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_insertq_Vdq_Udq }       // SSE4A
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f79Q[4] = {
  /* -- */ { 0, &Ia_vmwrite_Gq_Eq },        // VMX
  /* 66 */ { 0, &Ia_extrq_Vdq_Uq },         // SSE4A
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_insertq_Vdq_Udq }       // SSE4A
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f7c[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_haddpd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_haddps_Vps_Wps }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f7d[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_hsubpd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_hsubps_Vps_Wps }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f7e[4] = {
  /* -- */ { 0, &Ia_movd_Ed_Pq },
  /* 66 */ { 0, &Ia_movd_Ed_Vd },
  /* F3 */ { 0, &Ia_movq_Vq_Wq },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f7eQ[4] = {
  /* -- */ { 0, &Ia_movq_Eq_Pq },
  /* 66 */ { 0, &Ia_movq_Eq_Vq },
  /* F2 */ { 0, &Ia_movq_Vq_Wq },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f7f[4] = {
  /* -- */ { 0, &Ia_movq_Qq_Pq },
  /* 66 */ { 0, &Ia_movdqa_Wdq_Vdq },
  /* F2 */ { 0, &Ia_movdqu_Wdq_Vdq },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fc2[4] = {
  /* -- */ { 0, &Ia_cmpps_Vps_Wps_Ib },
  /* 66 */ { 0, &Ia_cmppd_Vpd_Wpd_Ib },
  /* F3 */ { 0, &Ia_cmpss_Vss_Wss_Ib },
  /* F2 */ { 0, &Ia_cmpsd_Vsd_Wsd_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fc4[2] = {
  /* -- */ { 0, &Ia_pinsrw_Pq_Ew_Ib },
  /* 66 */ { 0, &Ia_pinsrw_Vdq_Ew_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fc5[2] = {
  /* -- */ { 0, &Ia_pextrw_Gd_Nq_Ib },
  /* 66 */ { 0, &Ia_pextrw_Gd_Udq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fc6[2] = {
  /* -- */ { 0, &Ia_shufps_Vps_Wps_Ib },
  /* 66 */ { 0, &Ia_shufpd_Vpd_Wpd_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd0[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_addsubpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_addsubps_Vps_Wps },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd1[2] = {
  /* -- */ { 0, &Ia_psrlw_Pq_Qq },
  /* 66 */ { 0, &Ia_psrlw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd2[2] = {
  /* -- */ { 0, &Ia_psrld_Pq_Qq },
  /* 66 */ { 0, &Ia_psrld_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd3[2] = {
  /* -- */ { 0, &Ia_psrlq_Pq_Qq },
  /* 66 */ { 0, &Ia_psrlq_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd4[2] = {
  /* -- */ { 0, &Ia_paddq_Pq_Qq },
  /* 66 */ { 0, &Ia_paddq_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd5[2] = {
  /* -- */ { 0, &Ia_pmullw_Pq_Qq },
  /* 66 */ { 0, &Ia_pmullw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd6[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_movq_Wq_Vq },
  /* F3 */ { 0, &Ia_movq2dq_Vdq_Qq },
  /* F2 */ { 0, &Ia_movdq2q_Pq_Udq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd7[2] = {
  /* -- */ { 0, &Ia_pmovmskb_Gd_Nq },
  /* 66 */ { 0, &Ia_pmovmskb_Gd_Udq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd8[2] = {
  /* -- */ { 0, &Ia_psubusb_Pq_Qq },
  /* 66 */ { 0, &Ia_psubusb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd9[2] = {
  /* -- */ { 0, &Ia_psubusw_Pq_Qq },
  /* 66 */ { 0, &Ia_psubusw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fda[2] = {
  /* -- */ { 0, &Ia_pminub_Pq_Qq },
  /* 66 */ { 0, &Ia_pminub_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fdb[2] = {
  /* -- */ { 0, &Ia_pand_Pq_Qq },
  /* 66 */ { 0, &Ia_pand_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fdc[2] = {
  /* -- */ { 0, &Ia_paddusb_Pq_Qq },
  /* 66 */ { 0, &Ia_paddusb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fdd[2] = {
  /* -- */ { 0, &Ia_paddusw_Pq_Qq },
  /* 66 */ { 0, &Ia_paddusw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fde[2] = {
  /* -- */ { 0, &Ia_pmaxub_Pq_Qq },
  /* 66 */ { 0, &Ia_pmaxub_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fdf[2] = {
  /* -- */ { 0, &Ia_pandn_Pq_Qq },
  /* 66 */ { 0, &Ia_pandn_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe0[2] = {
  /* -- */ { 0, &Ia_pavgb_Pq_Qq },
  /* 66 */ { 0, &Ia_pavgb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe1[2] = {
  /* -- */ { 0, &Ia_psraw_Pq_Qq },
  /* 66 */ { 0, &Ia_psraw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe2[2] = {
  /* -- */ { 0, &Ia_psrad_Pq_Qq },
  /* 66 */ { 0, &Ia_psrad_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe3[2] = {
  /* -- */ { 0, &Ia_pavgw_Pq_Qq },
  /* 66 */ { 0, &Ia_pavgw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe4[2] = {
  /* -- */ { 0, &Ia_pmulhuw_Pq_Qq },
  /* 66 */ { 0, &Ia_pmulhuw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe5[2] = {
  /* -- */ { 0, &Ia_pmulhw_Pq_Qq },
  /* 66 */ { 0, &Ia_pmulhw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe6[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_cvttpd2dq_Vq_Wpd },
  /* F3 */ { 0, &Ia_cvtdq2pd_Vpd_Wq },
  /* F2 */ { 0, &Ia_cvtpd2dq_Vq_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe7[2] = {
  /* -- */ { 0, &Ia_movntq_Mq_Pq },
  /* 66 */ { 0, &Ia_movntdq_Mdq_Vdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe8[2] = {
  /* -- */ { 0, &Ia_psubsb_Pq_Qq },
  /* 66 */ { 0, &Ia_psubsb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe9[2] = {
  /* -- */ { 0, &Ia_psubsw_Pq_Qq },
  /* 66 */ { 0, &Ia_psubsw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fea[2] = {
  /* -- */ { 0, &Ia_pminsw_Pq_Qq },
  /* 66 */ { 0, &Ia_pminsw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0feb[2] = {
  /* -- */ { 0, &Ia_por_Pq_Qq },
  /* 66 */ { 0, &Ia_por_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fec[2] = {
  /* -- */ { 0, &Ia_paddsb_Pq_Qq },
  /* 66 */ { 0, &Ia_paddsb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fed[2] = {
  /* -- */ { 0, &Ia_paddsw_Pq_Qq },
  /* 66 */ { 0, &Ia_paddsw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fee[2] = {
  /* -- */ { 0, &Ia_pmaxsw_Pq_Qq },
  /* 66 */ { 0, &Ia_pmaxsw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fef[2] = {
  /* -- */ { 0, &Ia_pxor_Pq_Qq },
  /* 66 */ { 0, &Ia_pxor_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff1[2] = {
  /* -- */ { 0, &Ia_psllw_Pq_Qq },
  /* 66 */ { 0, &Ia_psllw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff2[2] = {
  /* -- */ { 0, &Ia_pslld_Pq_Qq },
  /* 66 */ { 0, &Ia_pslld_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff3[2] = {
  /* -- */ { 0, &Ia_psllq_Pq_Qq },
  /* 66 */ { 0, &Ia_psllq_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff4[2] = {
  /* -- */ { 0, &Ia_pmuludq_Pq_Qq },
  /* 66 */ { 0, &Ia_pmuludq_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff5[2] = {
  /* -- */ { 0, &Ia_pmaddwd_Pq_Qq },
  /* 66 */ { 0, &Ia_pmaddwd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff6[2] = {
  /* -- */ { 0, &Ia_psadbw_Pq_Qq },
  /* 66 */ { 0, &Ia_psadbw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff7[2] = {
  /* -- */ { 0, &Ia_maskmovq_Pq_Nq },
  /* 66 */ { 0, &Ia_maskmovdqu_Vdq_Udq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff8[2] = {
  /* -- */ { 0, &Ia_psubb_Pq_Qq },
  /* 66 */ { 0, &Ia_psubb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff9[2] = {
  /* -- */ { 0, &Ia_psubw_Pq_Qq },
  /* 66 */ { 0, &Ia_psubw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ffa[2] = {
  /* -- */ { 0, &Ia_psubd_Pq_Qq },
  /* 66 */ { 0, &Ia_psubd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ffb[2] = {
  /* -- */ { 0, &Ia_psubq_Pq_Qq },
  /* 66 */ { 0, &Ia_psubq_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ffc[2] = {
  /* -- */ { 0, &Ia_paddb_Pq_Qq },
  /* 66 */ { 0, &Ia_paddb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ffd[2] = {
  /* -- */ { 0, &Ia_paddw_Pq_Qq },
  /* 66 */ { 0, &Ia_paddw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ffe[2] = {
  /* -- */ { 0, &Ia_paddd_Pq_Qq },
  /* 66 */ { 0, &Ia_paddd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1202[2] = {
  /* -- */ { 0, &Ia_psrlw_Nq_Ib },
  /* 66 */ { 0, &Ia_psrlw_Udq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1204[2] = {
  /* -- */ { 0, &Ia_psraw_Nq_Ib },
  /* 66 */ { 0, &Ia_psraw_Udq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1206[2] = {
  /* -- */ { 0, &Ia_psllw_Nq_Ib },
  /* 66 */ { 0, &Ia_psllw_Udq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1302[2] = {
  /* -- */ { 0, &Ia_psrld_Nq_Ib },
  /* 66 */ { 0, &Ia_psrld_Udq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1304[2] = {
  /* -- */ { 0, &Ia_psrad_Nq_Ib },
  /* 66 */ { 0, &Ia_psrad_Udq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1306[2] = {
  /* -- */ { 0, &Ia_pslld_Nq_Ib },
  /* 66 */ { 0, &Ia_pslld_Udq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1402[2] = {
  /* -- */ { 0, &Ia_psrlq_Nq_Ib },
  /* 66 */ { 0, &Ia_psrlq_Udq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1406[2] = {
  /* -- */ { 0, &Ia_psllq_Nq_Ib },
  /* 66 */ { 0, &Ia_psllq_Udq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3800[2] = {
  /* -- */ { 0, &Ia_pshufb_Pq_Qq },     // SSSE3
  /* 66 */ { 0, &Ia_pshufb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3801[2] = {
  /* -- */ { 0, &Ia_phaddw_Pq_Qq },     // SSSE3
  /* 66 */ { 0, &Ia_phaddw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3802[2] = {
  /* -- */ { 0, &Ia_phaddd_Pq_Qq },     // SSSE3
  /* 66 */ { 0, &Ia_phaddd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3803[2] = {
  /* -- */ { 0, &Ia_phaddsw_Pq_Qq },    // SSSE3
  /* 66 */ { 0, &Ia_phaddsw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3804[2] = {
  /* -- */ { 0, &Ia_pmaddubsw_Pq_Qq },  // SSSE3
  /* 66 */ { 0, &Ia_pmaddubsw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3805[2] = {
  /* -- */ { 0, &Ia_phsubw_Pq_Qq },     // SSSE3
  /* 66 */ { 0, &Ia_phsubw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3806[2] = {
  /* -- */ { 0, &Ia_phsubd_Pq_Qq },     // SSSE3
  /* 66 */ { 0, &Ia_phsubd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3807[2] = {
  /* -- */ { 0, &Ia_phsubsw_Pq_Qq },    // SSSE3
  /* 66 */ { 0, &Ia_phsubsw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3808[2] = {
  /* -- */ { 0, &Ia_psignb_Pq_Qq },     // SSSE3
  /* 66 */ { 0, &Ia_psignb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3809[2] = {
  /* -- */ { 0, &Ia_psignw_Pq_Qq },     // SSSE3
  /* 66 */ { 0, &Ia_psignw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f380a[2] = {
  /* -- */ { 0, &Ia_psignd_Pq_Qq },     // SSSE3
  /* 66 */ { 0, &Ia_psignd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f380b[2] = {
  /* -- */ { 0, &Ia_pmulhrsw_Pq_Qq },   // SSSE3
  /* 66 */ { 0, &Ia_pmulhrsw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f381c[2] = {
  /* -- */ { 0, &Ia_pabsb_Pq_Qq },      // SSSE3
  /* 66 */ { 0, &Ia_pabsb_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f381d[2] = {
  /* -- */ { 0, &Ia_pabsw_Pq_Qq },      // SSSE3
  /* 66 */ { 0, &Ia_pabsw_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f381e[2] = {
  /* -- */ { 0, &Ia_pabsd_Pq_Qq },      // SSSE3
  /* 66 */ { 0, &Ia_pabsd_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f38f0w[4] = {
  /* -- */ { 0, &Ia_movbe_Gw_Mw },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_crc32_Gd_Eb }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f38f0d[4] = {
  /* -- */ { 0, &Ia_movbe_Gd_Md },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_crc32_Gd_Eb }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f38f0q[4] = {
  /* -- */ { 0, &Ia_movbe_Gq_Mq },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_crc32_Gd_Eb }
};

static BxDisasmOpcodeTable_t BxDisasmGrpOs64B_0f38f0[3] = {
  /* 16 */ { GRPSSE(0f38f0w) },
  /* 32 */ { GRPSSE(0f38f0d) },
  /* 64 */ { GRPSSE(0f38f0q) },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f38f1w[4] = {
  /* -- */ { 0, &Ia_movbe_Mw_Gw },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_crc32_Gd_Ew }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f38f1d[4] = {
  /* -- */ { 0, &Ia_movbe_Md_Gd },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_crc32_Gd_Ed }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f38f1q[4] = {
  /* -- */ { 0, &Ia_movbe_Mq_Gq },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_crc32_Gd_Eq }
};

static BxDisasmOpcodeTable_t BxDisasmGrpOs64B_0f38f1[3] = {
  /* 16 */ { GRPSSE(0f38f1w) },
  /* 32 */ { GRPSSE(0f38f1d) },
  /* 64 */ { GRPSSE(0f38f1q) },
};

static BxDisasmOpcodeTable_t BxDisasmGrpOs64B_adcx[3] = {
  /* 16 */ { 0, &Ia_adcx_Gd_Ed },
  /* 32 */ { 0, &Ia_adcx_Gd_Ed },
  /* 64 */ { 0, &Ia_adcx_Gq_Eq },
};

static BxDisasmOpcodeTable_t BxDisasmGrpOs64B_adox[3] = {
  /* 16 */ { 0, &Ia_adox_Gd_Ed },
  /* 32 */ { 0, &Ia_adox_Gd_Ed },
  /* 64 */ { 0, &Ia_adox_Gq_Eq },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f38f6[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { GR64BIT(adcx) },
  /* F3 */ { GR64BIT(adox) },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3a0f[2] = {
  /* -- */ { 0, &Ia_palignr_Pq_Qq_Ib }, // SSSE3
  /* 66 */ { 0, &Ia_palignr_Vdq_Wdq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGrpOs64B_pextr[3] = {
  /* 16 */ { 0, &Ia_pextrd_Ed_Vdq_Ib },
  /* 32 */ { 0, &Ia_pextrd_Ed_Vdq_Ib },
  /* 64 */ { 0, &Ia_pextrq_Eq_Vdq_Ib },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3a16[2] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { GR64BIT(pextr) }          // SSE4
};

static BxDisasmOpcodeTable_t BxDisasmGrpOs64B_pinsr[3] = {
  /* 16 */ { 0, &Ia_pinsrd_Vdq_Ed_Ib },
  /* 32 */ { 0, &Ia_pinsrd_Vdq_Ed_Ib },
  /* 64 */ { 0, &Ia_pinsrq_Vdq_Eq_Ib },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3a22[2] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { GR64BIT(pinsr) }          // SSE4
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fbcw[4] = {
  /* -- */ { 0, &Ia_bsfw_Gw_Ew },
  /* 66 */ { 0, &Ia_bsfw_Gw_Ew },
  /* F3 */ { 0, &Ia_tzcntw_Gw_Ew },
  /* F2 */ { 0, &Ia_bsfw_Gw_Ew }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fbcd[4] = {
  /* -- */ { 0, &Ia_bsfl_Gd_Ed },
  /* 66 */ { 0, &Ia_bsfl_Gd_Ed },
  /* F3 */ { 0, &Ia_tzcntl_Gd_Ed },
  /* F2 */ { 0, &Ia_bsfl_Gd_Ed }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fbcq[4] = {
  /* -- */ { 0, &Ia_bsfq_Gq_Eq },
  /* 66 */ { 0, &Ia_bsfq_Gq_Eq },
  /* F3 */ { 0, &Ia_tzcntq_Gq_Eq },
  /* F2 */ { 0, &Ia_bsfq_Gq_Eq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fbdw[4] = {
  /* -- */ { 0, &Ia_bsrw_Gw_Ew },
  /* 66 */ { 0, &Ia_bsrw_Gw_Ew },
  /* F3 */ { 0, &Ia_lzcntw_Gw_Ew },
  /* F2 */ { 0, &Ia_bsrw_Gw_Ew }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fbdd[4] = {
  /* -- */ { 0, &Ia_bsrl_Gd_Ed },
  /* 66 */ { 0, &Ia_bsrl_Gd_Ed },
  /* F3 */ { 0, &Ia_lzcntl_Gd_Ed },
  /* F2 */ { 0, &Ia_bsrl_Gd_Ed }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fbdq[4] = {
  /* -- */ { 0, &Ia_bsrq_Gq_Eq },
  /* 66 */ { 0, &Ia_bsrq_Gq_Eq },
  /* F3 */ { 0, &Ia_lzcntq_Gq_Eq },
  /* F2 */ { 0, &Ia_bsrq_Gq_Eq }
};

/* ************************************************************************ */
/* 3-byte opcode table (Table A-4, 0F 38) */

static BxDisasmOpcodeTable_t BxDisasm3ByteOpTable0f38[256] = {
  /* 00 */ { GRPSSE2(0f3800) },
  /* 01 */ { GRPSSE2(0f3801) },
  /* 02 */ { GRPSSE2(0f3802) },
  /* 03 */ { GRPSSE2(0f3803) },
  /* 04 */ { GRPSSE2(0f3804) },
  /* 05 */ { GRPSSE2(0f3805) },
  /* 06 */ { GRPSSE2(0f3806) },
  /* 07 */ { GRPSSE2(0f3807) },
  /* 08 */ { GRPSSE2(0f3808) },
  /* 09 */ { GRPSSE2(0f3809) },
  /* 0A */ { GRPSSE2(0f380a) },
  /* 0B */ { GRPSSE2(0f380b) },
  /* 0C */ { 0, &Ia_Invalid },
  /* 0D */ { 0, &Ia_Invalid },
  /* 0E */ { 0, &Ia_Invalid },
  /* 0F */ { 0, &Ia_Invalid },
  /* 10 */ { GRPSSE66(Ia_pblendvb_Vdq_Wdq) },
  /* 11 */ { 0, &Ia_Invalid },
  /* 12 */ { 0, &Ia_Invalid },
  /* 13 */ { 0, &Ia_Invalid },
  /* 14 */ { GRPSSE66(Ia_blendvps_Vps_Wps) },
  /* 15 */ { GRPSSE66(Ia_blendvpd_Vpd_Wpd) },
  /* 16 */ { 0, &Ia_Invalid },
  /* 17 */ { GRPSSE66(Ia_ptest_Vdq_Wdq) },
  /* 18 */ { 0, &Ia_Invalid },
  /* 19 */ { 0, &Ia_Invalid },
  /* 1A */ { 0, &Ia_Invalid },
  /* 1B */ { 0, &Ia_Invalid },
  /* 1C */ { GRPSSE2(0f381c) },
  /* 1D */ { GRPSSE2(0f381d) },
  /* 1E */ { GRPSSE2(0f381e) },
  /* 1F */ { 0, &Ia_Invalid },
  /* 20 */ { GRPSSE66(Ia_pmovsxbw_Vdq_Wq) },
  /* 21 */ { GRPSSE66(Ia_pmovsxbd_Vdq_Wd) },
  /* 22 */ { GRPSSE66(Ia_pmovsxbq_Vdq_Ww) },
  /* 23 */ { GRPSSE66(Ia_pmovsxwd_Vdq_Wq) },
  /* 24 */ { GRPSSE66(Ia_pmovsxwq_Vdq_Wd) },
  /* 25 */ { GRPSSE66(Ia_pmovsxdq_Vdq_Wq) },
  /* 26 */ { 0, &Ia_Invalid },
  /* 27 */ { 0, &Ia_Invalid },
  /* 28 */ { GRPSSE66(Ia_pmuldq_Vdq_Wdq) },
  /* 29 */ { GRPSSE66(Ia_pcmpeqq_Vdq_Wdq) },
  /* 2A */ { GRPSSE66(Ia_movntdqa_Vdq_Mdq) },
  /* 2B */ { GRPSSE66(Ia_packusdw_Vdq_Wdq) },
  /* 2C */ { 0, &Ia_Invalid },
  /* 2D */ { 0, &Ia_Invalid },
  /* 2E */ { 0, &Ia_Invalid },
  /* 2F */ { 0, &Ia_Invalid },
  /* 30 */ { GRPSSE66(Ia_pmovzxbw_Vdq_Wq) },
  /* 31 */ { GRPSSE66(Ia_pmovzxbd_Vdq_Wd) },
  /* 32 */ { GRPSSE66(Ia_pmovzxbq_Vdq_Ww) },
  /* 33 */ { GRPSSE66(Ia_pmovzxwd_Vdq_Wq) },
  /* 34 */ { GRPSSE66(Ia_pmovzxwq_Vdq_Wd) },
  /* 35 */ { GRPSSE66(Ia_pmovzxdq_Vdq_Wq) },
  /* 36 */ { 0, &Ia_Invalid },
  /* 37 */ { GRPSSE66(Ia_pcmpgtq_Vdq_Wdq) },
  /* 38 */ { GRPSSE66(Ia_pminsb_Vdq_Wdq) },
  /* 39 */ { GRPSSE66(Ia_pminsd_Vdq_Wdq) },
  /* 3A */ { GRPSSE66(Ia_pminuw_Vdq_Wdq) },
  /* 3B */ { GRPSSE66(Ia_pminud_Vdq_Wdq) },
  /* 3C */ { GRPSSE66(Ia_pmaxsb_Vdq_Wdq) },
  /* 3D */ { GRPSSE66(Ia_pmaxsd_Vdq_Wdq) },
  /* 3E */ { GRPSSE66(Ia_pmaxuw_Vdq_Wdq) },
  /* 3F */ { GRPSSE66(Ia_pmaxud_Vdq_Wdq) },
  /* 40 */ { GRPSSE66(Ia_pmulld_Vdq_Wdq) },
  /* 41 */ { GRPSSE66(Ia_phminposuw_Vdq_Wdq) },
  /* 42 */ { 0, &Ia_Invalid },
  /* 43 */ { 0, &Ia_Invalid },
  /* 44 */ { 0, &Ia_Invalid },
  /* 45 */ { 0, &Ia_Invalid },
  /* 46 */ { 0, &Ia_Invalid },
  /* 47 */ { 0, &Ia_Invalid },
  /* 48 */ { 0, &Ia_Invalid },
  /* 49 */ { 0, &Ia_Invalid },
  /* 4A */ { 0, &Ia_Invalid },
  /* 4B */ { 0, &Ia_Invalid },
  /* 4C */ { 0, &Ia_Invalid },
  /* 4D */ { 0, &Ia_Invalid },
  /* 4E */ { 0, &Ia_Invalid },
  /* 4F */ { 0, &Ia_Invalid },
  /* 50 */ { 0, &Ia_Invalid },
  /* 51 */ { 0, &Ia_Invalid },
  /* 52 */ { 0, &Ia_Invalid },
  /* 53 */ { 0, &Ia_Invalid },
  /* 54 */ { 0, &Ia_Invalid },
  /* 55 */ { 0, &Ia_Invalid },
  /* 56 */ { 0, &Ia_Invalid },
  /* 57 */ { 0, &Ia_Invalid },
  /* 58 */ { 0, &Ia_Invalid },
  /* 59 */ { 0, &Ia_Invalid },
  /* 5A */ { 0, &Ia_Invalid },
  /* 5B */ { 0, &Ia_Invalid },
  /* 5C */ { 0, &Ia_Invalid },
  /* 5D */ { 0, &Ia_Invalid },
  /* 5E */ { 0, &Ia_Invalid },
  /* 5F */ { 0, &Ia_Invalid },
  /* 60 */ { 0, &Ia_Invalid },
  /* 61 */ { 0, &Ia_Invalid },
  /* 62 */ { 0, &Ia_Invalid },
  /* 63 */ { 0, &Ia_Invalid },
  /* 64 */ { 0, &Ia_Invalid },
  /* 65 */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_Invalid },
  /* 67 */ { 0, &Ia_Invalid },
  /* 68 */ { 0, &Ia_Invalid },
  /* 69 */ { 0, &Ia_Invalid },
  /* 6A */ { 0, &Ia_Invalid },
  /* 6B */ { 0, &Ia_Invalid },
  /* 6C */ { 0, &Ia_Invalid },
  /* 6D */ { 0, &Ia_Invalid },
  /* 6E */ { 0, &Ia_Invalid },
  /* 6F */ { 0, &Ia_Invalid },
  /* 70 */ { 0, &Ia_Invalid },
  /* 71 */ { 0, &Ia_Invalid },
  /* 72 */ { 0, &Ia_Invalid },
  /* 73 */ { 0, &Ia_Invalid },
  /* 74 */ { 0, &Ia_Invalid },
  /* 75 */ { 0, &Ia_Invalid },
  /* 76 */ { 0, &Ia_Invalid },
  /* 77 */ { 0, &Ia_Invalid },
  /* 78 */ { 0, &Ia_Invalid },
  /* 79 */ { 0, &Ia_Invalid },
  /* 7A */ { 0, &Ia_Invalid },
  /* 7B */ { 0, &Ia_Invalid },
  /* 7C */ { 0, &Ia_Invalid },
  /* 7D */ { 0, &Ia_Invalid },
  /* 7E */ { 0, &Ia_Invalid },
  /* 7F */ { 0, &Ia_Invalid },
  /* 80 */ { GRPSSE66(Ia_invept_Gy_Mdq) },
  /* 81 */ { GRPSSE66(Ia_invvpid_Gy_Mdq) },
  /* 82 */ { GRPSSE66(Ia_invpcid_Gy_Mdq) },
  /* 83 */ { 0, &Ia_Invalid },
  /* 84 */ { 0, &Ia_Invalid },
  /* 85 */ { 0, &Ia_Invalid },
  /* 86 */ { 0, &Ia_Invalid },
  /* 87 */ { 0, &Ia_Invalid },
  /* 88 */ { 0, &Ia_Invalid },
  /* 89 */ { 0, &Ia_Invalid },
  /* 8A */ { 0, &Ia_Invalid },
  /* 8B */ { 0, &Ia_Invalid },
  /* 8C */ { 0, &Ia_Invalid },
  /* 8D */ { 0, &Ia_Invalid },
  /* 8E */ { 0, &Ia_Invalid },
  /* 8F */ { 0, &Ia_Invalid },
  /* 90 */ { 0, &Ia_Invalid },
  /* 91 */ { 0, &Ia_Invalid },
  /* 92 */ { 0, &Ia_Invalid },
  /* 93 */ { 0, &Ia_Invalid },
  /* 94 */ { 0, &Ia_Invalid },
  /* 95 */ { 0, &Ia_Invalid },
  /* 96 */ { 0, &Ia_Invalid },
  /* 97 */ { 0, &Ia_Invalid },
  /* 98 */ { 0, &Ia_Invalid },
  /* 99 */ { 0, &Ia_Invalid },
  /* 9A */ { 0, &Ia_Invalid },
  /* 9B */ { 0, &Ia_Invalid },
  /* 9C */ { 0, &Ia_Invalid },
  /* 9D */ { 0, &Ia_Invalid },
  /* 9E */ { 0, &Ia_Invalid },
  /* 9F */ { 0, &Ia_Invalid },
  /* A0 */ { 0, &Ia_Invalid },
  /* A1 */ { 0, &Ia_Invalid },
  /* A2 */ { 0, &Ia_Invalid },
  /* A3 */ { 0, &Ia_Invalid },
  /* A4 */ { 0, &Ia_Invalid },
  /* A5 */ { 0, &Ia_Invalid },
  /* A6 */ { 0, &Ia_Invalid },
  /* A7 */ { 0, &Ia_Invalid },
  /* A8 */ { 0, &Ia_Invalid },
  /* A9 */ { 0, &Ia_Invalid },
  /* AA */ { 0, &Ia_Invalid },
  /* AB */ { 0, &Ia_Invalid },
  /* AC */ { 0, &Ia_Invalid },
  /* AD */ { 0, &Ia_Invalid },
  /* AE */ { 0, &Ia_Invalid },
  /* AF */ { 0, &Ia_Invalid },
  /* B0 */ { 0, &Ia_Invalid },
  /* B1 */ { 0, &Ia_Invalid },
  /* B2 */ { 0, &Ia_Invalid },
  /* B3 */ { 0, &Ia_Invalid },
  /* B4 */ { 0, &Ia_Invalid },
  /* B5 */ { 0, &Ia_Invalid },
  /* B6 */ { 0, &Ia_Invalid },
  /* B7 */ { 0, &Ia_Invalid },
  /* B8 */ { 0, &Ia_Invalid },
  /* B9 */ { 0, &Ia_Invalid },
  /* BA */ { 0, &Ia_Invalid },
  /* BB */ { 0, &Ia_Invalid },
  /* BC */ { 0, &Ia_Invalid },
  /* BD */ { 0, &Ia_Invalid },
  /* BE */ { 0, &Ia_Invalid },
  /* BF */ { 0, &Ia_Invalid },
  /* C0 */ { 0, &Ia_Invalid },
  /* C1 */ { 0, &Ia_Invalid },
  /* C2 */ { 0, &Ia_Invalid },
  /* C3 */ { 0, &Ia_Invalid },
  /* C4 */ { 0, &Ia_Invalid },
  /* C5 */ { 0, &Ia_Invalid },
  /* C6 */ { 0, &Ia_Invalid },
  /* C7 */ { 0, &Ia_Invalid },
  /* C8 */ { GRPSSENONE(Ia_sha1nexte_Vdq_Wdq) },
  /* C9 */ { GRPSSENONE(Ia_sha1msg1_Vdq_Wdq) },
  /* CA */ { GRPSSENONE(Ia_sha1msg2_Vdq_Wdq) },
  /* CB */ { GRPSSENONE(Ia_sha256rnds2_Vdq_Wdq) },
  /* CC */ { GRPSSENONE(Ia_sha256msg1_Vdq_Wdq) },
  /* CD */ { GRPSSENONE(Ia_sha256msg2_Vdq_Wdq) },
  /* CE */ { 0, &Ia_Invalid },
  /* CF */ { 0, &Ia_Invalid },
  /* D0 */ { 0, &Ia_Invalid },
  /* D1 */ { 0, &Ia_Invalid },
  /* D2 */ { 0, &Ia_Invalid },
  /* D3 */ { 0, &Ia_Invalid },
  /* D4 */ { 0, &Ia_Invalid },
  /* D5 */ { 0, &Ia_Invalid },
  /* D6 */ { 0, &Ia_Invalid },
  /* D7 */ { 0, &Ia_Invalid },
  /* D8 */ { 0, &Ia_Invalid },
  /* D9 */ { 0, &Ia_Invalid },
  /* DA */ { 0, &Ia_Invalid },
  /* DB */ { GRPSSE66(Ia_aesimc_Vdq_Wdq) },
  /* DC */ { GRPSSE66(Ia_aesenc_Vdq_Wdq) },
  /* DD */ { GRPSSE66(Ia_aesenclast_Vdq_Wdq) },
  /* DE */ { GRPSSE66(Ia_aesdec_Vdq_Wdq) },
  /* DF */ { GRPSSE66(Ia_aesdeclast_Vdq_Wdq) },
  /* E0 */ { 0, &Ia_Invalid },
  /* E1 */ { 0, &Ia_Invalid },
  /* E2 */ { 0, &Ia_Invalid },
  /* E3 */ { 0, &Ia_Invalid },
  /* E4 */ { 0, &Ia_Invalid },
  /* E5 */ { 0, &Ia_Invalid },
  /* E6 */ { 0, &Ia_Invalid },
  /* E7 */ { 0, &Ia_Invalid },
  /* E8 */ { 0, &Ia_Invalid },
  /* E9 */ { 0, &Ia_Invalid },
  /* EA */ { 0, &Ia_Invalid },
  /* EB */ { 0, &Ia_Invalid },
  /* EC */ { 0, &Ia_Invalid },
  /* ED */ { 0, &Ia_Invalid },
  /* EE */ { 0, &Ia_Invalid },
  /* EF */ { 0, &Ia_Invalid },
  /* F0 */ { GR64BIT(0f38f0) },
  /* F1 */ { GR64BIT(0f38f1) },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid },
  /* F4 */ { 0, &Ia_Invalid },
  /* F5 */ { 0, &Ia_Invalid },
  /* F6 */ { GRPSSE(0f38f6) },
  /* F7 */ { 0, &Ia_Invalid },
  /* F8 */ { 0, &Ia_Invalid },
  /* F9 */ { 0, &Ia_Invalid },
  /* FA */ { 0, &Ia_Invalid },
  /* FB */ { 0, &Ia_Invalid },
  /* FC */ { 0, &Ia_Invalid },
  /* FD */ { 0, &Ia_Invalid },
  /* FE */ { 0, &Ia_Invalid },
  /* FF */ { 0, &Ia_Invalid }
};

/* ************************************************************************ */
/* 3-byte opcode table (Table A-5, 0F 3A) */

static BxDisasmOpcodeTable_t BxDisasm3ByteOpTable0f3a[256] = {
  /* 00 */ { 0, &Ia_Invalid },
  /* 01 */ { 0, &Ia_Invalid },
  /* 02 */ { 0, &Ia_Invalid },
  /* 03 */ { 0, &Ia_Invalid },
  /* 04 */ { 0, &Ia_Invalid },
  /* 05 */ { 0, &Ia_Invalid },
  /* 06 */ { 0, &Ia_Invalid },
  /* 07 */ { 0, &Ia_Invalid },
  /* 08 */ { GRPSSE66(Ia_roundps_Vps_Wps_Ib) },
  /* 09 */ { GRPSSE66(Ia_roundpd_Vpd_Wpd_Ib) },
  /* 0A */ { GRPSSE66(Ia_roundss_Vss_Wss_Ib) },
  /* 0B */ { GRPSSE66(Ia_roundsd_Vsd_Wsd_Ib) },
  /* 0C */ { GRPSSE66(Ia_blendps_Vps_Wps_Ib) },
  /* 0D */ { GRPSSE66(Ia_blendpd_Vpd_Wpd_Ib) },
  /* 0E */ { GRPSSE66(Ia_pblendw_Vdq_Wdq_Ib) },
  /* 0F */ { GRPSSE2(0f3a0f) },
  /* 10 */ { 0, &Ia_Invalid },
  /* 11 */ { 0, &Ia_Invalid },
  /* 12 */ { 0, &Ia_Invalid },
  /* 13 */ { 0, &Ia_Invalid },
  /* 14 */ { GRPSSE66(Ia_pextrb_Ebd_Vdq_Ib) },
  /* 15 */ { GRPSSE66(Ia_pextrw_Ewd_Vdq_Ib) },
  /* 16 */ { GRPSSE2(0f3a16) },
  /* 17 */ { GRPSSE66(Ia_extractps_Ed_Vdq_Ib) },
  /* 18 */ { 0, &Ia_Invalid },
  /* 19 */ { 0, &Ia_Invalid },
  /* 1A */ { 0, &Ia_Invalid },
  /* 1B */ { 0, &Ia_Invalid },
  /* 1C */ { 0, &Ia_Invalid },
  /* 1D */ { 0, &Ia_Invalid },
  /* 1E */ { 0, &Ia_Invalid },
  /* 1F */ { 0, &Ia_Invalid },
  /* 20 */ { GRPSSE66(Ia_pinsrb_Vdq_Ew_Ib) },
  /* 21 */ { GRPSSE66(Ia_insertps_Vps_Wss_Ib) },
  /* 22 */ { GRPSSE2(0f3a22) },
  /* 23 */ { 0, &Ia_Invalid },
  /* 24 */ { 0, &Ia_Invalid },
  /* 25 */ { 0, &Ia_Invalid },
  /* 26 */ { 0, &Ia_Invalid },
  /* 27 */ { 0, &Ia_Invalid },
  /* 28 */ { 0, &Ia_Invalid },
  /* 29 */ { 0, &Ia_Invalid },
  /* 2A */ { 0, &Ia_Invalid },
  /* 2B */ { 0, &Ia_Invalid },
  /* 2C */ { 0, &Ia_Invalid },
  /* 2D */ { 0, &Ia_Invalid },
  /* 2E */ { 0, &Ia_Invalid },
  /* 2F */ { 0, &Ia_Invalid },
  /* 30 */ { 0, &Ia_Invalid },
  /* 31 */ { 0, &Ia_Invalid },
  /* 32 */ { 0, &Ia_Invalid },
  /* 33 */ { 0, &Ia_Invalid },
  /* 34 */ { 0, &Ia_Invalid },
  /* 35 */ { 0, &Ia_Invalid },
  /* 36 */ { 0, &Ia_Invalid },
  /* 37 */ { 0, &Ia_Invalid },
  /* 38 */ { 0, &Ia_Invalid },
  /* 39 */ { 0, &Ia_Invalid },
  /* 3A */ { 0, &Ia_Invalid },
  /* 3B */ { 0, &Ia_Invalid },
  /* 3C */ { 0, &Ia_Invalid },
  /* 3D */ { 0, &Ia_Invalid },
  /* 3E */ { 0, &Ia_Invalid },
  /* 3F */ { 0, &Ia_Invalid },
  /* 40 */ { GRPSSE66(Ia_dpps_Vps_Wps_Ib) },
  /* 41 */ { GRPSSE66(Ia_dppd_Vpd_Wpd_Ib) },
  /* 42 */ { GRPSSE66(Ia_mpsadbw_Vdq_Wdq_Ib) },
  /* 43 */ { 0, &Ia_Invalid },
  /* 44 */ { GRPSSE66(Ia_pclmulqdq_Vdq_Wdq_Ib) },
  /* 45 */ { 0, &Ia_Invalid },
  /* 46 */ { 0, &Ia_Invalid },
  /* 47 */ { 0, &Ia_Invalid },
  /* 48 */ { 0, &Ia_Invalid },
  /* 49 */ { 0, &Ia_Invalid },
  /* 4A */ { 0, &Ia_Invalid },
  /* 4B */ { 0, &Ia_Invalid },
  /* 4C */ { 0, &Ia_Invalid },
  /* 4D */ { 0, &Ia_Invalid },
  /* 4E */ { 0, &Ia_Invalid },
  /* 4F */ { 0, &Ia_Invalid },
  /* 50 */ { 0, &Ia_Invalid },
  /* 51 */ { 0, &Ia_Invalid },
  /* 52 */ { 0, &Ia_Invalid },
  /* 53 */ { 0, &Ia_Invalid },
  /* 54 */ { 0, &Ia_Invalid },
  /* 55 */ { 0, &Ia_Invalid },
  /* 56 */ { 0, &Ia_Invalid },
  /* 57 */ { 0, &Ia_Invalid },
  /* 58 */ { 0, &Ia_Invalid },
  /* 59 */ { 0, &Ia_Invalid },
  /* 5A */ { 0, &Ia_Invalid },
  /* 5B */ { 0, &Ia_Invalid },
  /* 5C */ { 0, &Ia_Invalid },
  /* 5D */ { 0, &Ia_Invalid },
  /* 5E */ { 0, &Ia_Invalid },
  /* 5F */ { 0, &Ia_Invalid },
  /* 60 */ { GRPSSE66(Ia_pcmpestrm_Vdq_Wdq_Ib) },
  /* 61 */ { GRPSSE66(Ia_pcmpestri_Vdq_Wdq_Ib) },
  /* 62 */ { GRPSSE66(Ia_pcmpistrm_Vdq_Wdq_Ib) },
  /* 63 */ { GRPSSE66(Ia_pcmpistri_Vdq_Wdq_Ib) },
  /* 64 */ { 0, &Ia_Invalid },
  /* 65 */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_Invalid },
  /* 67 */ { 0, &Ia_Invalid },
  /* 68 */ { 0, &Ia_Invalid },
  /* 69 */ { 0, &Ia_Invalid },
  /* 6A */ { 0, &Ia_Invalid },
  /* 6B */ { 0, &Ia_Invalid },
  /* 6C */ { 0, &Ia_Invalid },
  /* 6D */ { 0, &Ia_Invalid },
  /* 6E */ { 0, &Ia_Invalid },
  /* 6F */ { 0, &Ia_Invalid },
  /* 70 */ { 0, &Ia_Invalid },
  /* 71 */ { 0, &Ia_Invalid },
  /* 72 */ { 0, &Ia_Invalid },
  /* 73 */ { 0, &Ia_Invalid },
  /* 74 */ { 0, &Ia_Invalid },
  /* 75 */ { 0, &Ia_Invalid },
  /* 76 */ { 0, &Ia_Invalid },
  /* 77 */ { 0, &Ia_Invalid },
  /* 78 */ { 0, &Ia_Invalid },
  /* 79 */ { 0, &Ia_Invalid },
  /* 7A */ { 0, &Ia_Invalid },
  /* 7B */ { 0, &Ia_Invalid },
  /* 7C */ { 0, &Ia_Invalid },
  /* 7D */ { 0, &Ia_Invalid },
  /* 7E */ { 0, &Ia_Invalid },
  /* 7F */ { 0, &Ia_Invalid },
  /* 80 */ { 0, &Ia_Invalid },
  /* 81 */ { 0, &Ia_Invalid },
  /* 82 */ { 0, &Ia_Invalid },
  /* 83 */ { 0, &Ia_Invalid },
  /* 84 */ { 0, &Ia_Invalid },
  /* 85 */ { 0, &Ia_Invalid },
  /* 86 */ { 0, &Ia_Invalid },
  /* 87 */ { 0, &Ia_Invalid },
  /* 88 */ { 0, &Ia_Invalid },
  /* 89 */ { 0, &Ia_Invalid },
  /* 8A */ { 0, &Ia_Invalid },
  /* 8B */ { 0, &Ia_Invalid },
  /* 8C */ { 0, &Ia_Invalid },
  /* 8D */ { 0, &Ia_Invalid },
  /* 8E */ { 0, &Ia_Invalid },
  /* 8F */ { 0, &Ia_Invalid },
  /* 90 */ { 0, &Ia_Invalid },
  /* 91 */ { 0, &Ia_Invalid },
  /* 92 */ { 0, &Ia_Invalid },
  /* 93 */ { 0, &Ia_Invalid },
  /* 94 */ { 0, &Ia_Invalid },
  /* 95 */ { 0, &Ia_Invalid },
  /* 96 */ { 0, &Ia_Invalid },
  /* 97 */ { 0, &Ia_Invalid },
  /* 98 */ { 0, &Ia_Invalid },
  /* 99 */ { 0, &Ia_Invalid },
  /* 9A */ { 0, &Ia_Invalid },
  /* 9B */ { 0, &Ia_Invalid },
  /* 9C */ { 0, &Ia_Invalid },
  /* 9D */ { 0, &Ia_Invalid },
  /* 9E */ { 0, &Ia_Invalid },
  /* 9F */ { 0, &Ia_Invalid },
  /* A0 */ { 0, &Ia_Invalid },
  /* A1 */ { 0, &Ia_Invalid },
  /* A2 */ { 0, &Ia_Invalid },
  /* A3 */ { 0, &Ia_Invalid },
  /* A4 */ { 0, &Ia_Invalid },
  /* A5 */ { 0, &Ia_Invalid },
  /* A6 */ { 0, &Ia_Invalid },
  /* A7 */ { 0, &Ia_Invalid },
  /* A8 */ { 0, &Ia_Invalid },
  /* A9 */ { 0, &Ia_Invalid },
  /* AA */ { 0, &Ia_Invalid },
  /* AB */ { 0, &Ia_Invalid },
  /* AC */ { 0, &Ia_Invalid },
  /* AD */ { 0, &Ia_Invalid },
  /* AE */ { 0, &Ia_Invalid },
  /* AF */ { 0, &Ia_Invalid },
  /* B0 */ { 0, &Ia_Invalid },
  /* B1 */ { 0, &Ia_Invalid },
  /* B2 */ { 0, &Ia_Invalid },
  /* B3 */ { 0, &Ia_Invalid },
  /* B4 */ { 0, &Ia_Invalid },
  /* B5 */ { 0, &Ia_Invalid },
  /* B6 */ { 0, &Ia_Invalid },
  /* B7 */ { 0, &Ia_Invalid },
  /* B8 */ { 0, &Ia_Invalid },
  /* B9 */ { 0, &Ia_Invalid },
  /* BA */ { 0, &Ia_Invalid },
  /* BB */ { 0, &Ia_Invalid },
  /* BC */ { 0, &Ia_Invalid },
  /* BD */ { 0, &Ia_Invalid },
  /* BE */ { 0, &Ia_Invalid },
  /* BF */ { 0, &Ia_Invalid },
  /* C0 */ { 0, &Ia_Invalid },
  /* C1 */ { 0, &Ia_Invalid },
  /* C2 */ { 0, &Ia_Invalid },
  /* C3 */ { 0, &Ia_Invalid },
  /* C4 */ { 0, &Ia_Invalid },
  /* C5 */ { 0, &Ia_Invalid },
  /* C6 */ { 0, &Ia_Invalid },
  /* C7 */ { 0, &Ia_Invalid },
  /* C8 */ { 0, &Ia_Invalid },
  /* C9 */ { 0, &Ia_Invalid },
  /* CA */ { 0, &Ia_Invalid },
  /* CB */ { 0, &Ia_Invalid },
  /* CC */ { GRPSSENONE(Ia_sha1rnds4_Vdq_Wdq_Ib) },
  /* CD */ { 0, &Ia_Invalid },
  /* CE */ { 0, &Ia_Invalid },
  /* CF */ { 0, &Ia_Invalid },
  /* D0 */ { 0, &Ia_Invalid },
  /* D1 */ { 0, &Ia_Invalid },
  /* D2 */ { 0, &Ia_Invalid },
  /* D3 */ { 0, &Ia_Invalid },
  /* D4 */ { 0, &Ia_Invalid },
  /* D5 */ { 0, &Ia_Invalid },
  /* D6 */ { 0, &Ia_Invalid },
  /* D7 */ { 0, &Ia_Invalid },
  /* D8 */ { 0, &Ia_Invalid },
  /* D9 */ { 0, &Ia_Invalid },
  /* DA */ { 0, &Ia_Invalid },
  /* DB */ { 0, &Ia_Invalid },
  /* DC */ { 0, &Ia_Invalid },
  /* DD */ { 0, &Ia_Invalid },
  /* DE */ { 0, &Ia_Invalid },
  /* DF */ { GRPSSE66(Ia_aeskeygenassist_Vdq_Wdq_Ib) },
  /* E0 */ { 0, &Ia_Invalid },
  /* E1 */ { 0, &Ia_Invalid },
  /* E2 */ { 0, &Ia_Invalid },
  /* E3 */ { 0, &Ia_Invalid },
  /* E4 */ { 0, &Ia_Invalid },
  /* E5 */ { 0, &Ia_Invalid },
  /* E6 */ { 0, &Ia_Invalid },
  /* E7 */ { 0, &Ia_Invalid },
  /* E8 */ { 0, &Ia_Invalid },
  /* E9 */ { 0, &Ia_Invalid },
  /* EA */ { 0, &Ia_Invalid },
  /* EB */ { 0, &Ia_Invalid },
  /* EC */ { 0, &Ia_Invalid },
  /* ED */ { 0, &Ia_Invalid },
  /* EE */ { 0, &Ia_Invalid },
  /* EF */ { 0, &Ia_Invalid },
  /* F0 */ { 0, &Ia_Invalid },
  /* F1 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid },
  /* F4 */ { 0, &Ia_Invalid },
  /* F5 */ { 0, &Ia_Invalid },
  /* F6 */ { 0, &Ia_Invalid },
  /* F7 */ { 0, &Ia_Invalid },
  /* F8 */ { 0, &Ia_Invalid },
  /* F9 */ { 0, &Ia_Invalid },
  /* FA */ { 0, &Ia_Invalid },
  /* FB */ { 0, &Ia_Invalid },
  /* FC */ { 0, &Ia_Invalid },
  /* FD */ { 0, &Ia_Invalid },
  /* FE */ { 0, &Ia_Invalid },
  /* FF */ { 0, &Ia_Invalid }
};
