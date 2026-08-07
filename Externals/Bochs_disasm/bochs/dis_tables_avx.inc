/////////////////////////////////////////////////////////////////////////
// $Id: dis_tables_avx.inc 11878 2013-10-11 20:09:51Z sshwarts $
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2011 Stanislav Shwartsman
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
/* AVX VEX-encoded 0x0F, 0x0F 0x38 and 0x0F 0x3A opcodes */

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f10[4] = {
  /* -- */ { 0, &Ia_vmovups_Vps_Wps },
  /* 66 */ { 0, &Ia_vmovupd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_vmovss_Vss_Wss },
  /* F2 */ { 0, &Ia_vmovsd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f11[4] = {
  /* -- */ { 0, &Ia_vmovups_Wps_Vps },
  /* 66 */ { 0, &Ia_vmovupd_Wpd_Vpd },
  /* F3 */ { 0, &Ia_vmovss_Wss_Vss },
  /* F2 */ { 0, &Ia_vmovsd_Wsd_Vsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupModVMOVHLPS[2] = {
  /* R */ { 0, &Ia_vmovhlps_Vps_Hdq_Udq },
  /* M */ { 0, &Ia_vmovlps_Vps_Hdq_Mq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f12[4] = {
  /* -- */ { GRPMOD(VMOVHLPS) },
  /* 66 */ { 0, &Ia_vmovlpd_Vpd_Hdq_Mq },
  /* F3 */ { 0, &Ia_vmovsldup_Vdq_Wdq },
  /* F2 */ { 0, &Ia_vmovddup_Vdq_Wq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f13[2] = {
  /* -- */ { 0, &Ia_vmovlps_Mq_Vps },
  /* 66 */ { 0, &Ia_vmovlpd_Mq_Vpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f14[2] = {
  /* -- */ { 0, &Ia_vunpcklps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vunpcklpd_Vpd_Hpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f15[2] = {
  /* -- */ { 0, &Ia_vunpckhps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vunpckhpd_Vpd_Hpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupModVMOVLHPS[2] = {
  /* R */ { 0, &Ia_vmovlhps_Vps_Hdq_Udq },
  /* M */ { 0, &Ia_vmovhps_Vps_Hdq_Mq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f16[4] = {
  /* -- */ { GRPMOD(VMOVLHPS) },
  /* 66 */ { 0, &Ia_vmovhpd_Vpd_Hdq_Mq },
  /* F3 */ { 0, &Ia_vmovshdup_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f17[2] = {
  /* -- */ { 0, &Ia_vmovhps_Mq_Vps },
  /* 66 */ { 0, &Ia_vmovhpd_Mq_Vpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f28[2] = {
  /* -- */ { 0, &Ia_vmovaps_Vps_Wps },
  /* 66 */ { 0, &Ia_vmovapd_Vpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f29[2] = {
  /* -- */ { 0, &Ia_vmovaps_Wps_Vps },
  /* 66 */ { 0, &Ia_vmovapd_Wpd_Vpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f2a[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_vcvtsi2ss_Vss_Hps_Ey },
  /* F2 */ { 0, &Ia_vcvtsi2sd_Vsd_Hpd_Ey }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f2b[2] = {
  /* -- */ { 0, &Ia_vmovntps_Mps_Vps },
  /* 66 */ { 0, &Ia_vmovntpd_Mpd_Vpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f2c[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_vcvttss2si_Gy_Wss },
  /* F2 */ { 0, &Ia_vcvttsd2si_Gy_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f2d[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_vcvtss2si_Gy_Wss },
  /* F2 */ { 0, &Ia_vcvtsd2si_Gy_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f2e[2] = {
  /* -- */ { 0, &Ia_vucomiss_Vss_Wss },
  /* 66 */ { 0, &Ia_vucomisd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f2f[2] = {
  /* -- */ { 0, &Ia_vcomiss_Vss_Wss },
  /* 66 */ { 0, &Ia_vcomisd_Vsd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f50[2] = {
  /* -- */ { 0, &Ia_vmovmskps_Gd_Ups },
  /* 66 */ { 0, &Ia_vmovmskpd_Gd_Upd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f51[4] = {
  /* -- */ { 0, &Ia_vsqrtps_Vps_Wps },
  /* 66 */ { 0, &Ia_vsqrtpd_Vpd_Wpd },
  /* F3 */ { 0, &Ia_vsqrtss_Vss_Hps_Wss },
  /* F2 */ { 0, &Ia_vsqrtsd_Vsd_Hpd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f52[4] = {
  /* -- */ { 0, &Ia_vrsqrtps_Vps_Wps },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_vrsqrtss_Vss_Hps_Wss },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f53[4] = {
  /* -- */ { 0, &Ia_vrcpps_Vps_Wps },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_vrcpss_Vss_Hps_Wss },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f54[2] = {
  /* -- */ { 0, &Ia_vandps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vandpd_Vpd_Hpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f55[2] = {
  /* -- */ { 0, &Ia_vandnps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vandnpd_Vpd_Hpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f56[2] = {
  /* -- */ { 0, &Ia_vorps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vorpd_Vpd_Hpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f57[2] = {
  /* -- */ { 0, &Ia_vxorps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vxorpd_Vpd_Hpd_Wpd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f58[4] = {
  /* -- */ { 0, &Ia_vaddps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vaddpd_Vpd_Hpd_Wpd },
  /* F3 */ { 0, &Ia_vaddss_Vss_Hps_Wss },
  /* F2 */ { 0, &Ia_vaddsd_Vsd_Hpd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f59[4] = {
  /* -- */ { 0, &Ia_vmulps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vmulpd_Vpd_Hpd_Wpd },
  /* F3 */ { 0, &Ia_vmulss_Vss_Hps_Wss },
  /* F2 */ { 0, &Ia_vmulsd_Vsd_Hpd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f5a[4] = {
  /* -- */ { 0, &Ia_vcvtps2pd_Vpd_Wps },
  /* 66 */ { 0, &Ia_vcvtpd2ps_Vps_Wpd },
  /* F3 */ { 0, &Ia_vcvtss2sd_Vsd_Hpd_Wss },
  /* F2 */ { 0, &Ia_vcvtsd2ss_Vss_Hps_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f5b[4] = {
  /* -- */ { 0, &Ia_vcvtdq2ps_Vps_Wdq },
  /* 66 */ { 0, &Ia_vcvtps2dq_Vdq_Wps },
  /* F3 */ { 0, &Ia_vcvttps2dq_Vdq_Wps },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f5c[4] = {
  /* -- */ { 0, &Ia_vsubps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vsubpd_Vpd_Hpd_Wpd },
  /* F3 */ { 0, &Ia_vsubss_Vss_Hps_Wss },
  /* F2 */ { 0, &Ia_vsubsd_Vsd_Hpd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f5d[4] = {
  /* -- */ { 0, &Ia_vminps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vminpd_Vpd_Hpd_Wpd },
  /* F3 */ { 0, &Ia_vminss_Vss_Hps_Wss },
  /* F2 */ { 0, &Ia_vminsd_Vsd_Hpd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f5e[4] = {
  /* -- */ { 0, &Ia_vdivps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vdivpd_Vpd_Hpd_Wpd },
  /* F3 */ { 0, &Ia_vdivss_Vss_Hps_Wss },
  /* F2 */ { 0, &Ia_vdivsd_Vsd_Hpd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f5f[4] = {
  /* -- */ { 0, &Ia_vmaxps_Vps_Hps_Wps },
  /* 66 */ { 0, &Ia_vmaxpd_Vpd_Hpd_Wpd },
  /* F3 */ { 0, &Ia_vmaxss_Vss_Hps_Wss },
  /* F2 */ { 0, &Ia_vmaxsd_Vsd_Hpd_Wsd }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_AVX0f6e[3] = {
  /* 0 */ { GRPSSE66(Ia_vmovd_Vdq_Ed) },
  /* 1 */ { GRPSSE66(Ia_vmovq_Vdq_Eq) }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f6f[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_vmovdqa_Vdq_Wdq },
  /* F3 */ { 0, &Ia_vmovdqu_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f70[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_vpshufd_Vdq_Hdq_Wdq_Ib },
  /* F3 */ { 0, &Ia_vpshufhw_Vdq_Hdq_Wdq_Ib },
  /* F2 */ { 0, &Ia_vpshuflw_Vdq_Hdq_Wdq_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f7c[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_vhaddpd_Vpd_Hpd_Wpd },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_vhaddps_Vps_Hps_Wps }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f7d[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_vhsubpd_Vpd_Hpd_Wpd },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_vhsubps_Vps_Hps_Wps }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_AVX660f7e[3] = {
  /* 0 */ { 0, &Ia_vmovd_Ed_Vd },
  /* 1 */ { 0, &Ia_vmovq_Eq_Vq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f7e[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { GRPVEXW(AVX660f7e) },
  /* F3 */ { 0, &Ia_vmovq_Vq_Wq },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f7f[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_vmovdqa_Wdq_Vdq },
  /* F3 */ { 0, &Ia_vmovdqu_Wdq_Vdq },
  /* F2 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0fc2[4] = {
  /* -- */ { 0, &Ia_vcmpps_Vps_Hps_Wps_Ib },
  /* 66 */ { 0, &Ia_vcmppd_Vpd_Hpd_Wpd_Ib },
  /* F3 */ { 0, &Ia_vcmpss_Vss_Hps_Wss_Ib },
  /* F2 */ { 0, &Ia_vcmpsd_Vsd_Hpd_Wsd_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0fc6[2] = {
  /* -- */ { 0, &Ia_vshufps_Vps_Hps_Wps_Ib },
  /* 66 */ { 0, &Ia_vshufpd_Vpd_Hpd_Wpd_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0fd0[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_vaddsubpd_Vpd_Hpd_Wpd },
  /* F3 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_vaddsubps_Vps_Hps_Wps }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0fe6[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_vcvttpd2dq_Vq_Wpd },
  /* F3 */ { 0, &Ia_vcvtdq2pd_Vpd_Wq },
  /* F2 */ { 0, &Ia_vcvtpd2dq_Vq_Wpd }
};

/* Group12 */
static BxDisasmOpcodeTable_t BxDisasmGroupAVXG12[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { GRPSSE66(Ia_vpsrlw_Hdq_Udq_Ib) },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { GRPSSE66(Ia_vpsraw_Hdq_Udq_Ib) },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { GRPSSE66(Ia_vpsllw_Hdq_Udq_Ib) },
  /* 7 */ { 0, &Ia_Invalid }
};

/* Group13 */
static BxDisasmOpcodeTable_t BxDisasmGroupAVXG13[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { GRPSSE66(Ia_vpsrld_Hdq_Udq_Ib) },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { GRPSSE66(Ia_vpsrad_Hdq_Udq_Ib) },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { GRPSSE66(Ia_vpslld_Hdq_Udq_Ib) },
  /* 7 */ { 0, &Ia_Invalid }
};

/* Group14 */
static BxDisasmOpcodeTable_t BxDisasmGroupAVXG14[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { GRPSSE66(Ia_vpsrlq_Hdq_Udq_Ib) },
  /* 3 */ { GRPSSE66(Ia_vpsrldq_Hdq_Udq_Ib) },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { GRPSSE66(Ia_vpsllq_Hdq_Udq_Ib) },
  /* 7 */ { GRPSSE66(Ia_vpslldq_Hdq_Udq_Ib) }
};

/* Group15 */
static BxDisasmOpcodeTable_t BxDisasmGroupAVXG15M[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { 0, &Ia_vldmxcsr },
  /* 3 */ { 0, &Ia_vstmxcsr },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { 0, &Ia_Invalid },
  /* 7 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupModAVXG15[2] = {
  /* R */ { 0, &Ia_Invalid },
  /* M */ { GRPN(AVXG15M) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a16[2] = {
  /* 0 */ { GRPSSE66(Ia_vpextrd_Ed_Vdq_Ib) },
  /* 1 */ { GRPSSE66(Ia_vpextrq_Eq_Vdq_Ib) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a22[2] = {
  /* 0 */ { GRPSSE66(Ia_vpinsrd_Vdq_Hdq_Ed_Ib) },
  /* 1 */ { GRPSSE66(Ia_vpinsrq_Vdq_Hdq_Eq_Ib) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a00[2] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { GRPSSE66(Ia_vpermq_Vdq_Wdq_Ib) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a01[2] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { GRPSSE66(Ia_vpermpd_Vpd_Wpd_Ib) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3845[2] = {
  /* 0 */ { GRPSSE66(Ia_vpsrlvd_Vdq_Hdq_Wdq) },
  /* 1 */ { GRPSSE66(Ia_vpsrlvq_Vdq_Hdq_Wdq) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3847[2] = {
  /* 0 */ { GRPSSE66(Ia_vpsllvd_Vdq_Hdq_Wdq) },
  /* 1 */ { GRPSSE66(Ia_vpsllvq_Vdq_Hdq_Wdq) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f388c[2] = {
  /* 0 */ { GRPSSE66(Ia_vmaskmovd_Vdq_Hdq_Mdq) },
  /* 1 */ { GRPSSE66(Ia_vmaskmovq_Vdq_Hdq_Mdq) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f388e[2] = {
  /* 0 */ { GRPSSE66(Ia_vmaskmovd_Mdq_Hdq_Vdq) },
  /* 1 */ { GRPSSE66(Ia_vmaskmovq_Mdq_Hdq_Vdq) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3890[2] = {
  /* 0 */ { GRPSSE66(Ia_vgatherdd_Vdq_VSib_Hdq) },
  /* 1 */ { GRPSSE66(Ia_vgatherdq_Vdq_VSib_Hdq) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3891[2] = {
  /* 0 */ { GRPSSE66(Ia_vgatherqd_Vdq_VSib_Hdq) },
  /* 1 */ { GRPSSE66(Ia_vgatherqq_Vdq_VSib_Hdq) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3892[2] = {
  /* 0 */ { GRPSSE66(Ia_vgatherdps_Vps_VSib_Hps) },
  /* 1 */ { GRPSSE66(Ia_vgatherdpd_Vpd_VSib_Hpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3893[2] = {
  /* 0 */ { GRPSSE66(Ia_vgatherqps_Vps_VSib_Hps) },
  /* 1 */ { GRPSSE66(Ia_vgatherqpd_Vpd_VSib_Hpd) }
};

// FMA
static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3896[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmaddsub132ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmaddsub132pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3897[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsubadd132ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmsubadd132pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3898[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmadd132ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmadd132pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3899[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmadd132ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfmadd132sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f389a[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsub132ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmsub132pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f389b[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsub132ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfmsub132sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f389c[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmadd132ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfnmadd132pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f389d[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmadd132ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfnmadd132sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f389e[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmsub132ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfnmsub132pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f389f[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmsub132ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfnmsub132sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38a6[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmaddsub213ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmaddsub213pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38a7[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsubadd213ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmsubadd213pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38a8[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmadd213ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmadd213pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38a9[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmadd213ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfmadd213sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38aa[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsub213ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmsub213pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38ab[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsub213ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfmsub213sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38ac[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmadd213ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfnmadd213pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38ad[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmadd213ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfnmadd213sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38ae[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmsub213ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfnmsub213pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38af[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmsub213ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfnmsub213sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38b6[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmaddsub231ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmaddsub231pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38b7[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsubadd231ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmsubadd231pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38b8[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmadd231ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmadd231pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38b9[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmadd231ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfmadd231sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38ba[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsub231ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfmsub231pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38bb[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsub231ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfmsub231sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38bc[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmadd231ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfnmadd231pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38bd[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmadd231ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfnmadd231sd_Vpd_Hsd_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38be[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmsub231ps_Vps_Hps_Wps) },
  /* 1 */ { GRPSSE66(Ia_vfnmsub231pd_Vpd_Hpd_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f38bf[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmsub231ss_Vps_Hss_Wss) },
  /* 1 */ { GRPSSE66(Ia_vfnmsub231sd_Vpd_Hsd_Wsd) }
};
// FMA

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f38f5[4] = {
  /* -- */ { 0, &Ia_bzhi_Gy_Ey_By },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_pext_Gy_By_Ey },
  /* F2 */ { 0, &Ia_pdep_Gy_By_Ey }
};

static BxDisasmOpcodeTable_t BxDisasmGroupAVX_0f38f7[4] = {
  /* -- */ { 0, &Ia_bextr_Gy_Ey_By },
  /* 66 */ { 0, &Ia_shlx_Gy_Ey_By },
  /* F3 */ { 0, &Ia_sarx_Gy_Ey_By },
  /* F2 */ { 0, &Ia_shrx_Gy_Ey_By }
};

/* Group17 */
static BxDisasmOpcodeTable_t BxDisasmGroupAVXG17[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_blsr_By_Ey },
  /* 2 */ { 0, &Ia_blsmsk_By_Ey },
  /* 3 */ { 0, &Ia_blsi_By_Ey },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { 0, &Ia_Invalid },
  /* 7 */ { 0, &Ia_Invalid }
};

// FMA4 (AMD)
static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a5c[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmaddsubps_Vps_Hps_Wps_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmaddsubps_Vps_Hps_VIb_Wps) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a5d[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmaddsubpd_Vpd_Hpd_Wpd_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmaddsubpd_Vpd_Hpd_VIb_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a5e[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsubaddps_Vps_Hps_Wps_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmsubaddps_Vps_Hps_VIb_Wps) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a5f[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsubaddpd_Vpd_Hpd_Wpd_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmsubaddpd_Vpd_Hpd_VIb_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a68[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmaddps_Vps_Hps_Wps_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmaddps_Vps_Hps_VIb_Wps) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a69[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmaddpd_Vpd_Hpd_Wpd_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmaddpd_Vpd_Hpd_VIb_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a6a[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmaddss_Vss_Hss_Wss_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmaddss_Vss_Hss_VIb_Wss) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a6b[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmaddsd_Vsd_Hsd_Wsd_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmaddsd_Vsd_Hsd_VIb_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a6c[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsubps_Vps_Hps_Wps_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmsubps_Vps_Hps_VIb_Wps) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a6d[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsubpd_Vpd_Hpd_Wpd_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmsubpd_Vpd_Hpd_VIb_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a6e[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsubss_Vss_Hss_Wss_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmsubss_Vss_Hss_VIb_Wss) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a6f[2] = {
  /* 0 */ { GRPSSE66(Ia_vfmsubsd_Vsd_Hsd_Wsd_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfmsubsd_Vsd_Hsd_VIb_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a78[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmaddps_Vps_Hps_Wps_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfnmaddps_Vps_Hps_VIb_Wps) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a79[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmaddpd_Vpd_Hpd_Wpd_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfnmaddpd_Vpd_Hpd_VIb_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a7a[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmaddss_Vss_Hss_Wss_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfnmaddss_Vss_Hss_VIb_Wss) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a7b[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmaddsd_Vsd_Hsd_Wsd_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfnmaddsd_Vsd_Hsd_VIb_Wsd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a7c[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmsubps_Vps_Hps_Wps_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfnmsubps_Vps_Hps_VIb_Wps) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a7d[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmsubpd_Vpd_Hpd_Wpd_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfnmsubpd_Vpd_Hpd_VIb_Wpd) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a7e[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmsubss_Vss_Hss_Wss_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfnmsubss_Vss_Hss_VIb_Wss) }
};

static BxDisasmOpcodeTable_t BxDisasmGrpVexW_0f3a7f[2] = {
  /* 0 */ { GRPSSE66(Ia_vfnmsubsd_Vsd_Hsd_Wsd_VIb) },
  /* 1 */ { GRPSSE66(Ia_vfnmsubsd_Vsd_Hsd_VIb_Wsd) }
};
// FMA4 (AMD)

static BxDisasmOpcodeTable_t BxDisasmOpcodesAVX[256*3] = {
  // 256 entries for VEX-encoded 0x0F opcodes
  /* 00 */ { 0, &Ia_Invalid },
  /* 01 */ { 0, &Ia_Invalid },
  /* 02 */ { 0, &Ia_Invalid },
  /* 03 */ { 0, &Ia_Invalid },
  /* 04 */ { 0, &Ia_Invalid },
  /* 05 */ { 0, &Ia_Invalid },
  /* 06 */ { 0, &Ia_Invalid },
  /* 07 */ { 0, &Ia_Invalid },
  /* 08 */ { 0, &Ia_Invalid },
  /* 09 */ { 0, &Ia_Invalid },
  /* 0A */ { 0, &Ia_Invalid },
  /* 0B */ { 0, &Ia_Invalid },
  /* 0C */ { 0, &Ia_Invalid },
  /* 0D */ { 0, &Ia_Invalid },
  /* 0E */ { 0, &Ia_Invalid },
  /* 0F */ { 0, &Ia_Invalid },
  /* 10 */ { GRPAVX(0f10) },
  /* 11 */ { GRPAVX(0f11) },
  /* 12 */ { GRPAVX(0f12) },
  /* 13 */ { GRPAVX2(0f13) },
  /* 14 */ { GRPAVX2(0f14) },
  /* 15 */ { GRPAVX2(0f15) },
  /* 16 */ { GRPAVX(0f16) },
  /* 17 */ { GRPAVX2(0f17) },
  /* 18 */ { 0, &Ia_Invalid },
  /* 19 */ { 0, &Ia_Invalid },
  /* 1A */ { 0, &Ia_Invalid },
  /* 1B */ { 0, &Ia_Invalid },
  /* 1C */ { 0, &Ia_Invalid },
  /* 1D */ { 0, &Ia_Invalid },
  /* 1E */ { 0, &Ia_Invalid },
  /* 1F */ { 0, &Ia_Invalid },
  /* 20 */ { 0, &Ia_Invalid },
  /* 21 */ { 0, &Ia_Invalid },
  /* 22 */ { 0, &Ia_Invalid },
  /* 23 */ { 0, &Ia_Invalid },
  /* 24 */ { 0, &Ia_Invalid },
  /* 25 */ { 0, &Ia_Invalid },
  /* 26 */ { 0, &Ia_Invalid },
  /* 27 */ { 0, &Ia_Invalid },
  /* 28 */ { GRPAVX2(0f28) },
  /* 29 */ { GRPAVX2(0f29) },
  /* 2A */ { GRPAVX(0f2a) },
  /* 2B */ { GRPAVX2(0f2b) },
  /* 2C */ { GRPAVX(0f2c) },
  /* 2C */ { GRPAVX(0f2d) },
  /* 2E */ { GRPAVX2(0f2e) },
  /* 2F */ { GRPAVX2(0f2f) },
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
  /* 40 */ { 0, &Ia_Invalid },
  /* 41 */ { 0, &Ia_Invalid },
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
  /* 50 */ { GRPAVX2(0f50) },
  /* 51 */ { GRPAVX(0f51) },
  /* 52 */ { GRPAVX(0f52) },
  /* 53 */ { GRPAVX(0f53) },
  /* 54 */ { GRPAVX2(0f54) },
  /* 55 */ { GRPAVX2(0f55) },
  /* 56 */ { GRPAVX2(0f56) },
  /* 57 */ { GRPAVX2(0f57) },
  /* 58 */ { GRPAVX(0f58) },
  /* 59 */ { GRPAVX(0f59) },
  /* 5A */ { GRPAVX(0f5a) },
  /* 5B */ { GRPAVX(0f5b) },
  /* 5C */ { GRPAVX(0f5c) },
  /* 5D */ { GRPAVX(0f5d) },
  /* 5E */ { GRPAVX(0f5e) },
  /* 5F */ { GRPAVX(0f5f) },
  /* 60 */ { GRPSSE66(Ia_vpunpcklbw_Vdq_Hdq_Wdq) },
  /* 61 */ { GRPSSE66(Ia_vpunpcklwd_Vdq_Hdq_Wdq) },
  /* 62 */ { GRPSSE66(Ia_vpunpckldq_Vdq_Hdq_Wdq) },
  /* 63 */ { GRPSSE66(Ia_vpacksswb_Vdq_Hdq_Wdq) },
  /* 64 */ { GRPSSE66(Ia_vpcmpgtb_Vdq_Hdq_Wdq) },
  /* 65 */ { GRPSSE66(Ia_vpcmpgtw_Vdq_Hdq_Wdq) },
  /* 66 */ { GRPSSE66(Ia_vpcmpgtd_Vdq_Hdq_Wdq) },
  /* 67 */ { GRPSSE66(Ia_vpackuswb_Vdq_Hdq_Wdq) },
  /* 68 */ { GRPSSE66(Ia_vpunpckhbw_Vdq_Hdq_Wdq) },
  /* 69 */ { GRPSSE66(Ia_vpunpckhwd_Vdq_Hdq_Wdq) },
  /* 6A */ { GRPSSE66(Ia_vpunpckhdq_Vdq_Hdq_Wdq) },
  /* 6B */ { GRPSSE66(Ia_vpackssdw_Vdq_Hdq_Wdq) },
  /* 6C */ { GRPSSE66(Ia_vpunpcklqdq_Vdq_Hdq_Wdq) },
  /* 6D */ { GRPSSE66(Ia_vpunpckhqdq_Vdq_Hdq_Wdq) },
  /* 6E */ { GRPVEXW(AVX0f6e) },
  /* 6F */ { GRPAVX(0f6f) },
  /* 70 */ { GRPAVX(0f70) },
  /* 71 */ { GRPN(AVXG12) },
  /* 72 */ { GRPN(AVXG13) },
  /* 73 */ { GRPN(AVXG14) },
  /* 74 */ { GRPSSE66(Ia_vpcmpeqb_Vdq_Hdq_Wdq) },
  /* 75 */ { GRPSSE66(Ia_vpcmpeqw_Vdq_Hdq_Wdq) },
  /* 76 */ { GRPSSE66(Ia_vpcmpeqd_Vdq_Hdq_Wdq) },
  /* 77 */ { GRPSSENONE(Ia_vzeroupper) },
  /* 78 */ { 0, &Ia_Invalid },
  /* 79 */ { 0, &Ia_Invalid },
  /* 7A */ { 0, &Ia_Invalid },
  /* 7B */ { 0, &Ia_Invalid },
  /* 7C */ { GRPAVX(0f7c) },
  /* 7D */ { GRPAVX(0f7d) },
  /* 7E */ { GRPAVX(0f7e) },
  /* 7F */ { GRPAVX(0f7f) },
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
  /* AE */ { GRPMOD(AVXG15) },
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
  /* C2 */ { GRPAVX(0fc2) },
  /* C3 */ { 0, &Ia_Invalid },
  /* C4 */ { GRPSSE66(Ia_vpinsrw_Vdq_Hdq_Ew_Ib) },
  /* C5 */ { GRPSSE66(Ia_vpextrw_Gd_Udq_Ib) },
  /* C6 */ { GRPAVX2(0fc6) },
  /* C7 */ { 0, &Ia_Invalid },
  /* C8 */ { 0, &Ia_Invalid },
  /* C9 */ { 0, &Ia_Invalid },
  /* CA */ { 0, &Ia_Invalid },
  /* CB */ { 0, &Ia_Invalid },
  /* CC */ { 0, &Ia_Invalid },
  /* CD */ { 0, &Ia_Invalid },
  /* CE */ { 0, &Ia_Invalid },
  /* CF */ { 0, &Ia_Invalid },
  /* D0 */ { GRPAVX(0fd0) },
  /* D1 */ { GRPSSE66(Ia_vpsrlw_Vdq_Hdq_Wdq) },
  /* D2 */ { GRPSSE66(Ia_vpsrld_Vdq_Hdq_Wdq) },
  /* D3 */ { GRPSSE66(Ia_vpsrlq_Vdq_Hdq_Wdq) },
  /* D4 */ { GRPSSE66(Ia_vpaddq_Vdq_Hdq_Wdq) },
  /* D5 */ { GRPSSE66(Ia_vpmullw_Vdq_Hdq_Wdq) },
  /* D6 */ { GRPSSE66(Ia_vmovq_Wq_Vq) },
  /* D7 */ { GRPSSE66(Ia_vpmovmskb_Gd_Udq) },
  /* D8 */ { GRPSSE66(Ia_vpsubusb_Vdq_Hdq_Wdq) },
  /* D9 */ { GRPSSE66(Ia_vpsubusw_Vdq_Hdq_Wdq) },
  /* DA */ { GRPSSE66(Ia_vpminub_Vdq_Hdq_Wdq) },
  /* DB */ { GRPSSE66(Ia_vpand_Vdq_Hdq_Wdq) },
  /* DC */ { GRPSSE66(Ia_vpaddusb_Vdq_Hdq_Wdq) },
  /* DD */ { GRPSSE66(Ia_vpaddusw_Vdq_Hdq_Wdq) },
  /* DE */ { GRPSSE66(Ia_vpmaxub_Vdq_Hdq_Wdq) },
  /* DF */ { GRPSSE66(Ia_vpandn_Vdq_Hdq_Wdq) },
  /* E0 */ { GRPSSE66(Ia_vpavgb_Vdq_Hdq_Wdq) },
  /* E1 */ { GRPSSE66(Ia_vpsraw_Vdq_Hdq_Wdq) },
  /* E2 */ { GRPSSE66(Ia_vpsrad_Vdq_Hdq_Wdq) },
  /* E3 */ { GRPSSE66(Ia_vpavgw_Vdq_Hdq_Wdq) },
  /* E4 */ { GRPSSE66(Ia_vpmulhuw_Vdq_Hdq_Wdq) },
  /* E5 */ { GRPSSE66(Ia_vpmulhw_Vdq_Hdq_Wdq) },
  /* E6 */ { GRPAVX(0fe6) },
  /* E7 */ { GRPSSE66(Ia_vmovntdq_Mdq_Vdq) },
  /* E8 */ { GRPSSE66(Ia_vpsubsb_Vdq_Hdq_Wdq) },
  /* E9 */ { GRPSSE66(Ia_vpsubsw_Vdq_Hdq_Wdq) },
  /* EA */ { GRPSSE66(Ia_vpminsw_Vdq_Hdq_Wdq) },
  /* EB */ { GRPSSE66(Ia_vpor_Vdq_Hdq_Wdq) },
  /* EC */ { GRPSSE66(Ia_vpaddsb_Vdq_Hdq_Wdq) },
  /* ED */ { GRPSSE66(Ia_vpaddsw_Vdq_Hdq_Wdq) },
  /* EE */ { GRPSSE66(Ia_vpmaxsw_Vdq_Hdq_Wdq) },
  /* EF */ { GRPSSE66(Ia_vpxor_Vdq_Hdq_Wdq) },
  /* F0 */ { GRPSSEF2(Ia_vlddqu_Vdq_Mdq) },
  /* F1 */ { GRPSSE66(Ia_vpsllw_Vdq_Hdq_Wdq) },
  /* F2 */ { GRPSSE66(Ia_vpslld_Vdq_Hdq_Wdq) },
  /* F3 */ { GRPSSE66(Ia_vpsllq_Vdq_Hdq_Wdq) },
  /* F4 */ { GRPSSE66(Ia_vpmuludq_Vdq_Hdq_Wdq) },
  /* F5 */ { GRPSSE66(Ia_vpmaddwd_Vdq_Hdq_Wdq) },
  /* F6 */ { GRPSSE66(Ia_vpsadbw_Vdq_Hdq_Wdq) },
  /* F7 */ { GRPSSE66(Ia_vmaskmovdqu_Vdq_Udq) },
  /* F8 */ { GRPSSE66(Ia_vpsubb_Vdq_Hdq_Wdq) },
  /* F9 */ { GRPSSE66(Ia_vpsubw_Vdq_Hdq_Wdq) },
  /* FA */ { GRPSSE66(Ia_vpsubd_Vdq_Hdq_Wdq) },
  /* FB */ { GRPSSE66(Ia_vpsubq_Vdq_Hdq_Wdq) },
  /* FC */ { GRPSSE66(Ia_vpaddb_Vdq_Hdq_Wdq) },
  /* FD */ { GRPSSE66(Ia_vpaddw_Vdq_Hdq_Wdq) },
  /* FE */ { GRPSSE66(Ia_vpaddd_Vdq_Hdq_Wdq) },
  /* FF */ { 0, &Ia_Invalid },

  // 256 entries for VEX-encoded 0x0F 0x38 opcodes
  /* 00 */ { GRPSSE66(Ia_vpshufb_Vdq_Hdq_Wdq) },
  /* 01 */ { GRPSSE66(Ia_vphaddw_Vdq_Hdq_Wdq) },
  /* 02 */ { GRPSSE66(Ia_vphaddd_Vdq_Hdq_Wdq) },
  /* 03 */ { GRPSSE66(Ia_vphaddsw_Vdq_Hdq_Wdq) },
  /* 04 */ { GRPSSE66(Ia_vpmaddubsw_Vdq_Hdq_Wdq) },
  /* 05 */ { GRPSSE66(Ia_vphsubw_Vdq_Hdq_Wdq) },
  /* 06 */ { GRPSSE66(Ia_vphsubd_Vdq_Hdq_Wdq) },
  /* 07 */ { GRPSSE66(Ia_vphsubsw_Vdq_Hdq_Wdq) },
  /* 08 */ { GRPSSE66(Ia_vpsignb_Vdq_Hdq_Wdq) },
  /* 09 */ { GRPSSE66(Ia_vpsignw_Vdq_Hdq_Wdq) },
  /* 0A */ { GRPSSE66(Ia_vpsignd_Vdq_Hdq_Wdq) },
  /* 0B */ { GRPSSE66(Ia_vpmulhrsw_Vdq_Hdq_Wdq) },
  /* 0C */ { GRPSSE66(Ia_vpermilps_Vps_Hps_Wps) },
  /* 0D */ { GRPSSE66(Ia_vpermilpd_Vpd_Hpd_Wpd) },
  /* 0E */ { GRPSSE66(Ia_vtestps_Vps_Wps) },
  /* 0F */ { GRPSSE66(Ia_vtestpd_Vpd_Wpd) },
  /* 10 */ { 0, &Ia_Invalid },
  /* 11 */ { 0, &Ia_Invalid },
  /* 12 */ { 0, &Ia_Invalid },
  /* 13 */ { GRPSSE66(Ia_vcvtph2ps_Vps_Wq) },
  /* 14 */ { 0, &Ia_Invalid },
  /* 15 */ { 0, &Ia_Invalid },
  /* 16 */ { GRPSSE66(Ia_vpermps_Vps_Hps_Wps) },
  /* 17 */ { GRPSSE66(Ia_vptest_Vdq_Wdq) },
  /* 18 */ { GRPSSE66(Ia_vbroadcastss_Vps_Wss) },
  /* 19 */ { GRPSSE66(Ia_vbroadcastsd_Vpd_Wsd) },
  /* 1A */ { GRPSSE66(Ia_vbroadcastf128_Vdq_Mdq) },
  /* 1B */ { 0, &Ia_Invalid },
  /* 1C */ { GRPSSE66(Ia_vpabsb_Vdq_Hdq_Wdq) },
  /* 1D */ { GRPSSE66(Ia_vpabsw_Vdq_Hdq_Wdq) },
  /* 1E */ { GRPSSE66(Ia_vpabsd_Vdq_Hdq_Wdq) },
  /* 1F */ { 0, &Ia_Invalid },
  /* 20 */ { GRPSSE66(Ia_vpmovsxbw_Vdq_Wq) },
  /* 21 */ { GRPSSE66(Ia_vpmovsxbd_Vdq_Wd) },
  /* 22 */ { GRPSSE66(Ia_vpmovsxbq_Vdq_Ww) },
  /* 23 */ { GRPSSE66(Ia_vpmovsxwd_Vdq_Wq) },
  /* 24 */ { GRPSSE66(Ia_vpmovsxwq_Vdq_Wd) },
  /* 25 */ { GRPSSE66(Ia_vpmovsxdq_Vdq_Wq) },
  /* 26 */ { 0, &Ia_Invalid },
  /* 27 */ { 0, &Ia_Invalid },
  /* 28 */ { GRPSSE66(Ia_vpmuldq_Vdq_Hdq_Wdq) },
  /* 29 */ { GRPSSE66(Ia_vpcmpeqq_Vdq_Hdq_Wdq) },
  /* 2A */ { GRPSSE66(Ia_vmovntdqa_Vdq_Mdq) },
  /* 2B */ { GRPSSE66(Ia_vpackusdw_Vdq_Hdq_Wdq) },
  /* 2C */ { GRPSSE66(Ia_vmaskmovps_Vps_Hps_Mps) },
  /* 2D */ { GRPSSE66(Ia_vmaskmovpd_Vpd_Hpd_Mpd) },
  /* 2E */ { GRPSSE66(Ia_vmaskmovps_Mps_Hps_Vps) },
  /* 2F */ { GRPSSE66(Ia_vmaskmovpd_Mpd_Hpd_Vpd) },
  /* 30 */ { GRPSSE66(Ia_vpmovzxbw_Vdq_Wq) },
  /* 31 */ { GRPSSE66(Ia_vpmovzxbd_Vdq_Wd) },
  /* 32 */ { GRPSSE66(Ia_vpmovzxbq_Vdq_Ww) },
  /* 33 */ { GRPSSE66(Ia_vpmovzxwd_Vdq_Wq) },
  /* 34 */ { GRPSSE66(Ia_vpmovzxwq_Vdq_Wd) },
  /* 35 */ { GRPSSE66(Ia_vpmovzxdq_Vdq_Wq) },
  /* 36 */ { GRPSSE66(Ia_vpermd_Vdq_Hdq_Wdq) },
  /* 37 */ { GRPSSE66(Ia_vpcmpgtq_Vdq_Hdq_Wdq) },
  /* 38 */ { GRPSSE66(Ia_vpminsb_Vdq_Hdq_Wdq) },
  /* 39 */ { GRPSSE66(Ia_vpminsd_Vdq_Hdq_Wdq) },
  /* 3A */ { GRPSSE66(Ia_vpminuw_Vdq_Hdq_Wdq) },
  /* 3B */ { GRPSSE66(Ia_vpminud_Vdq_Hdq_Wdq) },
  /* 3C */ { GRPSSE66(Ia_vpmaxsb_Vdq_Hdq_Wdq) },
  /* 3D */ { GRPSSE66(Ia_vpmaxsd_Vdq_Hdq_Wdq) },
  /* 3E */ { GRPSSE66(Ia_vpmaxuw_Vdq_Hdq_Wdq) },
  /* 3F */ { GRPSSE66(Ia_vpmaxud_Vdq_Hdq_Wdq) },
  /* 40 */ { GRPSSE66(Ia_vpmulld_Vdq_Hdq_Wdq) },
  /* 41 */ { GRPSSE66(Ia_vphminposuw_Vdq_Wdq) },
  /* 42 */ { 0, &Ia_Invalid },
  /* 43 */ { 0, &Ia_Invalid },
  /* 44 */ { 0, &Ia_Invalid },
  /* 45 */ { GRPVEXW(0f3845) },
  /* 46 */ { GRPSSE66(Ia_vpsravd_Vdq_Hdq_Wdq) },
  /* 47 */ { GRPVEXW(0f3847) },
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
  /* 58 */ { GRPSSE66(Ia_vpbroadcastd_Vdq_Wd) },
  /* 59 */ { GRPSSE66(Ia_vpbroadcastq_Vdq_Wq) },
  /* 5A */ { GRPSSE66(Ia_vbroadcasti128_Vdq_Mdq) },
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
  /* 78 */ { GRPSSE66(Ia_vpbroadcastb_Vdq_Wb) },
  /* 79 */ { GRPSSE66(Ia_vpbroadcastw_Vdq_Ww) },
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
  /* 8C */ { GRPVEXW(0f388c) },
  /* 8D */ { 0, &Ia_Invalid },
  /* 8E */ { GRPVEXW(0f388e) },
  /* 8F */ { 0, &Ia_Invalid },
  /* 90 */ { GRPVEXW(0f3890) },
  /* 91 */ { GRPVEXW(0f3891) },
  /* 92 */ { GRPVEXW(0f3892) },
  /* 93 */ { GRPVEXW(0f3893) },
  /* 94 */ { 0, &Ia_Invalid },
  /* 95 */ { 0, &Ia_Invalid },
  /* 96 */ { GRPVEXW(0f3896) },
  /* 97 */ { GRPVEXW(0f3897) },
  /* 98 */ { GRPVEXW(0f3898) },
  /* 99 */ { GRPVEXW(0f3899) },
  /* 9A */ { GRPVEXW(0f389a) },
  /* 9B */ { GRPVEXW(0f389b) },
  /* 9C */ { GRPVEXW(0f389c) },
  /* 9D */ { GRPVEXW(0f389d) },
  /* 9E */ { GRPVEXW(0f389e) },
  /* 9F */ { GRPVEXW(0f389f) },
  /* A0 */ { 0, &Ia_Invalid },
  /* A1 */ { 0, &Ia_Invalid },
  /* A2 */ { 0, &Ia_Invalid },
  /* A3 */ { 0, &Ia_Invalid },
  /* A4 */ { 0, &Ia_Invalid },
  /* A5 */ { 0, &Ia_Invalid },
  /* A6 */ { GRPVEXW(0f38a6) },
  /* A7 */ { GRPVEXW(0f38a7) },
  /* A8 */ { GRPVEXW(0f38a8) },
  /* A9 */ { GRPVEXW(0f38a9) },
  /* AA */ { GRPVEXW(0f38aa) },
  /* AB */ { GRPVEXW(0f38ab) },
  /* AC */ { GRPVEXW(0f38ac) },
  /* AD */ { GRPVEXW(0f38ad) },
  /* AE */ { GRPVEXW(0f38ae) },
  /* AF */ { GRPVEXW(0f38af) },
  /* B0 */ { 0, &Ia_Invalid },
  /* B1 */ { 0, &Ia_Invalid },
  /* B2 */ { 0, &Ia_Invalid },
  /* B3 */ { 0, &Ia_Invalid },
  /* B4 */ { 0, &Ia_Invalid },
  /* B5 */ { 0, &Ia_Invalid },
  /* B6 */ { GRPVEXW(0f38b6) },
  /* B7 */ { GRPVEXW(0f38b7) },
  /* B8 */ { GRPVEXW(0f38b8) },
  /* B9 */ { GRPVEXW(0f38b9) },
  /* BA */ { GRPVEXW(0f38ba) },
  /* BB */ { GRPVEXW(0f38bb) },
  /* BC */ { GRPVEXW(0f38bc) },
  /* BD */ { GRPVEXW(0f38bd) },
  /* BE */ { GRPVEXW(0f38be) },
  /* BF */ { GRPVEXW(0f38bf) },
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
  /* CC */ { 0, &Ia_Invalid },
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
  /* DB */ { GRPSSE66(Ia_vaesimc_Vdq_Wdq) },
  /* DC */ { GRPSSE66(Ia_vaesenc_Vdq_Hdq_Wdq) },
  /* DD */ { GRPSSE66(Ia_vaesenclast_Vdq_Hdq_Wdq) },
  /* DE */ { GRPSSE66(Ia_vaesdec_Vdq_Hdq_Wdq) },
  /* DF */ { GRPSSE66(Ia_vaesdeclast_Vdq_Hdq_Wdq) },
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
  /* F2 */ { 0, &Ia_andn_Gy_By_Ey },
  /* F3 */ { GRPN(AVXG17) },
  /* F4 */ { 0, &Ia_Invalid },
  /* F5 */ { GRPAVX(0f38f5) },
  /* F6 */ { GRPSSEF2(Ia_mulx_By_Gy_Ey) },
  /* F7 */ { GRPAVX(0f38f7) },
  /* F8 */ { 0, &Ia_Invalid },
  /* F9 */ { 0, &Ia_Invalid },
  /* FA */ { 0, &Ia_Invalid },
  /* FB */ { 0, &Ia_Invalid },
  /* FC */ { 0, &Ia_Invalid },
  /* FD */ { 0, &Ia_Invalid },
  /* FE */ { 0, &Ia_Invalid },
  /* FF */ { 0, &Ia_Invalid },

  // 256 entries for VEX-encoded 0x0F 0x3A opcodes
  /* 00 */ { GRPVEXW(0f3a00) },
  /* 01 */ { GRPVEXW(0f3a01) },
  /* 02 */ { GRPSSE66(Ia_vpblendd_Vdq_Hdq_Wdq_Ib) },
  /* 03 */ { 0, &Ia_Invalid },
  /* 04 */ { GRPSSE66(Ia_vpermilps_Vps_Wps_Ib) },
  /* 05 */ { GRPSSE66(Ia_vpermilpd_Vpd_Wpd_Ib) },
  /* 06 */ { GRPSSE66(Ia_vperm2f128_Vdq_Hdq_Wdq_Ib) },
  /* 07 */ { 0, &Ia_Invalid },
  /* 08 */ { GRPSSE66(Ia_vroundps_Vps_Wps_Ib) },
  /* 09 */ { GRPSSE66(Ia_vroundpd_Vpd_Wpd_Ib) },
  /* 0A */ { GRPSSE66(Ia_vroundss_Vss_Hps_Wss_Ib) },
  /* 0B */ { GRPSSE66(Ia_vroundsd_Vsd_Hpd_Wsd_Ib) },
  /* 0C */ { GRPSSE66(Ia_vblendps_Vps_Hps_Wps_Ib) },
  /* 0D */ { GRPSSE66(Ia_vblendpd_Vpd_Hpd_Wpd_Ib) },
  /* 0E */ { GRPSSE66(Ia_vpblendw_Vdq_Hdq_Wdq_Ib) },
  /* 0F */ { GRPSSE66(Ia_vpalignr_Vdq_Hdq_Wdq_Ib) },
  /* 10 */ { 0, &Ia_Invalid },
  /* 11 */ { 0, &Ia_Invalid },
  /* 12 */ { 0, &Ia_Invalid },
  /* 13 */ { 0, &Ia_Invalid },
  /* 14 */ { GRPSSE66(Ia_vpextrb_Ebd_Vdq_Ib) },
  /* 15 */ { GRPSSE66(Ia_vpextrw_Ewd_Vdq_Ib) },
  /* 16 */ { GRPVEXW(0f3a16) },
  /* 17 */ { GRPSSE66(Ia_vextractps_Ed_Vdq_Ib) },
  /* 18 */ { GRPSSE66(Ia_vinsertf128_Vdq_Hdq_Wdq_Ib) },
  /* 19 */ { GRPSSE66(Ia_vextractf128_Wdq_Vdq_Ib) },
  /* 1A */ { 0, &Ia_Invalid },
  /* 1B */ { 0, &Ia_Invalid },
  /* 1C */ { 0, &Ia_Invalid },
  /* 1D */ { GRPSSE66(Ia_vcvtps2ph_Wq_Vps_Ib) },
  /* 1E */ { 0, &Ia_Invalid },
  /* 1F */ { 0, &Ia_Invalid },
  /* 20 */ { GRPSSE66(Ia_vpinsrb_Vdq_Hdq_Ew_Ib) },
  /* 21 */ { GRPSSE66(Ia_vinsertps_Vps_Hps_Wss_Ib) },
  /* 22 */ { GRPVEXW(0f3a22) },
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
  /* 38 */ { GRPSSE66(Ia_vinserti128_Vdq_Hdq_Wdq_Ib) },
  /* 39 */ { GRPSSE66(Ia_vextracti128_Wdq_Vdq_Ib) },
  /* 3A */ { 0, &Ia_Invalid },
  /* 3B */ { 0, &Ia_Invalid },
  /* 3C */ { 0, &Ia_Invalid },
  /* 3D */ { 0, &Ia_Invalid },
  /* 3E */ { 0, &Ia_Invalid },
  /* 3F */ { 0, &Ia_Invalid },
  /* 40 */ { GRPSSE66(Ia_vdpps_Vps_Hps_Wps_Ib) },
  /* 41 */ { GRPSSE66(Ia_vdppd_Vpd_Hpd_Wpd_Ib) },
  /* 42 */ { GRPSSE66(Ia_vmpsadbw_Vdq_Hdq_Wdq_Ib) },
  /* 43 */ { 0, &Ia_Invalid },
  /* 44 */ { GRPSSE66(Ia_vpclmulqdq_Vdq_Hdq_Wdq_Ib) },
  /* 45 */ { 0, &Ia_Invalid },
  /* 46 */ { GRPSSE66(Ia_vperm2i128_Vdq_Hdq_Wdq_Ib) },
  /* 47 */ { 0, &Ia_Invalid },
  /* 48 */ { 0, &Ia_Invalid },
  /* 49 */ { 0, &Ia_Invalid },
  /* 4A */ { GRPSSE66(Ia_vblendvps_Vps_Hps_Wps_Ib) },
  /* 4B */ { GRPSSE66(Ia_vblendvpd_Vpd_Hpd_Wpd_Ib) },
  /* 4C */ { GRPSSE66(Ia_vpblendvb_Vdq_Hdq_Wdq_Ib) },
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
  /* 5C */ { GRPVEXW(0f3a5c) },
  /* 5D */ { GRPVEXW(0f3a5d) },
  /* 5E */ { GRPVEXW(0f3a5e) },
  /* 5F */ { GRPVEXW(0f3a5f) },
  /* 60 */ { GRPSSE66(Ia_vpcmpestrm_Vdq_Wdq_Ib) },
  /* 61 */ { GRPSSE66(Ia_vpcmpestri_Vdq_Wdq_Ib) },
  /* 62 */ { GRPSSE66(Ia_vpcmpistrm_Vdq_Wdq_Ib) },
  /* 63 */ { GRPSSE66(Ia_vpcmpistri_Vdq_Wdq_Ib) },
  /* 64 */ { 0, &Ia_Invalid },
  /* 65 */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_Invalid },
  /* 67 */ { 0, &Ia_Invalid },
  /* 68 */ { GRPVEXW(0f3a68) },
  /* 69 */ { GRPVEXW(0f3a69) },
  /* 6A */ { GRPVEXW(0f3a6a) },
  /* 6B */ { GRPVEXW(0f3a6b) },
  /* 6C */ { GRPVEXW(0f3a6c) },
  /* 6D */ { GRPVEXW(0f3a6d) },
  /* 6E */ { GRPVEXW(0f3a6e) },
  /* 6F */ { GRPVEXW(0f3a6f) },
  /* 70 */ { 0, &Ia_Invalid },
  /* 71 */ { 0, &Ia_Invalid },
  /* 72 */ { 0, &Ia_Invalid },
  /* 73 */ { 0, &Ia_Invalid },
  /* 74 */ { 0, &Ia_Invalid },
  /* 75 */ { 0, &Ia_Invalid },
  /* 76 */ { 0, &Ia_Invalid },
  /* 77 */ { 0, &Ia_Invalid },
  /* 78 */ { GRPVEXW(0f3a78) },
  /* 79 */ { GRPVEXW(0f3a79) },
  /* 7A */ { GRPVEXW(0f3a7a) },
  /* 7B */ { GRPVEXW(0f3a7b) },
  /* 7C */ { GRPVEXW(0f3a7c) },
  /* 7D */ { GRPVEXW(0f3a7d) },
  /* 7E */ { GRPVEXW(0f3a7e) },
  /* 7F */ { GRPVEXW(0f3a7f) },
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
  /* CC */ { 0, &Ia_Invalid },
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
  /* DF */ { GRPSSE66(Ia_vaeskeygenassist_Vdq_Wdq_Ib) },
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
  /* F0 */ { 0, &Ia_rorx_Gy_Ey_Ib },
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
