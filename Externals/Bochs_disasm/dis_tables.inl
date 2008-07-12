/* ************************************************************************ */
/* SSE opcodes */

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f10[4] = {
  /* -- */ { 0, &Ia_movups_Vps_Wps },
  /* 66 */ { 0, &Ia_movupd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_movsd_Vsd_Wsd  },
  /* F3 */ { 0, &Ia_movss_Vss_Wss  }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f11[4] = {
  /* -- */ { 0, &Ia_movups_Wps_Vps },
  /* 66 */ { 0, &Ia_movupd_Wpd_Vpd },
  /* F2 */ { 0, &Ia_movsd_Wsd_Vsd  },
  /* F3 */ { 0, &Ia_movss_Wss_Vss  }
};

static BxDisasmOpcodeTable_t BxDisasmGroupModMOVHLPS[2] = {
  /* R */ { 0, &Ia_movhlps_Vps_Uq },
  /* M */ { 0, &Ia_movlps_Vps_Mq  }
};

static BxDisasmOpcodeTable_t BxDisasmGroupModMOVHLPD[2] = {
  /* R */ { 0, &Ia_movhlpd_Vpd_Uq },
  /* M */ { 0, &Ia_movlpd_Vpd_Mq  }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f12[4] = {
  /* -- */ { GRPMOD(MOVHLPS) },
  /* 66 */ { GRPMOD(MOVHLPD) },
  /* F2 */ { 0, &Ia_movddup_Vdq_Wq   },
  /* F3 */ { 0, &Ia_movsldup_Vdq_Wdq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f13[4] = {
  /* -- */ { 0, &Ia_movlps_Mq_Vps },
  /* 66 */ { 0, &Ia_movlpd_Mq_Vpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};                        

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f14[4] = {
  /* -- */ { 0, &Ia_unpcklps_Vps_Wq },
  /* 66 */ { 0, &Ia_unpcklpd_Vpd_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f15[4] = {
  /* -- */ { 0, &Ia_unpckhps_Vps_Wdq },
  /* 66 */ { 0, &Ia_unpckhpd_Vpd_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupModMOVLHPS[2] = {
  /* R */ { 0, &Ia_movlhps_Vps_Uq },
  /* M */ { 0, &Ia_movhps_Vps_Mq  }
};

static BxDisasmOpcodeTable_t BxDisasmGroupModMOVLHPD[2] = {
  /* R */ { 0, &Ia_movlhpd_Vpd_Uq },
  /* M */ { 0, &Ia_movhpd_Vpd_Mq  }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f16[4] = {
  /* -- */ { GRPMOD(MOVLHPS) },
  /* 66 */ { GRPMOD(MOVLHPD) },
  /* F2 */ { 0, &Ia_Invalid  },
  /* F3 */ { 0, &Ia_movshdup_Vdq_Wdq },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f17[4] = {
  /* -- */ { 0, &Ia_movhps_Mq_Vps },
  /* 66 */ { 0, &Ia_movhpd_Mq_Vpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f28[4] = {
  /* -- */ { 0, &Ia_movaps_Vps_Wps },
  /* 66 */ { 0, &Ia_movapd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f29[4] = {
  /* -- */ { 0, &Ia_movaps_Wps_Vps },
  /* 66 */ { 0, &Ia_movapd_Wpd_Vpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2a[4] = {
  /* -- */ { 0, &Ia_cvtpi2ps_Vps_Qq },
  /* 66 */ { 0, &Ia_cvtpi2pd_Vpd_Qq },
  /* F2 */ { 0, &Ia_cvtsi2sd_Vsd_Ed },
  /* F3 */ { 0, &Ia_cvtsi2ss_Vss_Ed }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_640f2a[4] = {
  /* -- */ { 0, &Ia_cvtpi2ps_Vps_Qq },
  /* 66 */ { 0, &Ia_cvtpi2pd_Vpd_Qq },
  /* F2 */ { 0, &Ia_cvtsi2sd_Vsd_Eq },
  /* F3 */ { 0, &Ia_cvtsi2ss_Vss_Eq }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2b[4] = {
  /* -- */ { 0, &Ia_movntps_Mps_Vps },
  /* 66 */ { 0, &Ia_movntpd_Mpd_Vpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2c[4] = {
  /* -- */ { 0, &Ia_cvttps2pi_Pq_Wps },
  /* 66 */ { 0, &Ia_cvttpd2pi_Pq_Wpd },
  /* F2 */ { 0, &Ia_cvttsd2si_Gd_Wsd },
  /* F3 */ { 0, &Ia_cvttss2si_Gd_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2cQ[4] = {
  /* -- */ { 0, &Ia_cvttps2pi_Pq_Wps },
  /* 66 */ { 0, &Ia_cvttpd2pi_Pq_Wpd },
  /* F2 */ { 0, &Ia_cvttsd2si_Gq_Wsd },
  /* F3 */ { 0, &Ia_cvttss2si_Gq_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2d[4] = {
  /* -- */ { 0, &Ia_cvtps2pi_Pq_Wps },
  /* 66 */ { 0, &Ia_cvtpd2pi_Pq_Wpd },
  /* F2 */ { 0, &Ia_cvtsd2si_Gd_Wsd },
  /* F3 */ { 0, &Ia_cvtss2si_Gd_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2dQ[4] = {
  /* -- */ { 0, &Ia_cvtps2pi_Pq_Wps },
  /* 66 */ { 0, &Ia_cvtpd2pi_Pq_Wpd },
  /* F2 */ { 0, &Ia_cvtsd2si_Gq_Wsd },
  /* F3 */ { 0, &Ia_cvtss2si_Gq_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2e[4] = {
  /* -- */ { 0, &Ia_ucomiss_Vss_Wss },
  /* 66 */ { 0, &Ia_ucomisd_Vsd_Wss },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f2f[4] = {
  /* -- */ { 0, &Ia_comiss_Vss_Wss },
  /* 66 */ { 0, &Ia_comisd_Vsd_Wsd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3800[4] = {
  /* -- */ { 0, &Ia_pshufb_Pq_Qq },     // SSE4
  /* 66 */ { 0, &Ia_pshufb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3801[4] = {
  /* -- */ { 0, &Ia_phaddw_Pq_Qq },     // SSE4
  /* 66 */ { 0, &Ia_phaddw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3802[4] = {
  /* -- */ { 0, &Ia_phaddd_Pq_Qq },     // SSE4
  /* 66 */ { 0, &Ia_phaddd_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3803[4] = {
  /* -- */ { 0, &Ia_phaddsw_Pq_Qq },    // SSE4
  /* 66 */ { 0, &Ia_phaddsw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3804[4] = {
  /* -- */ { 0, &Ia_pmaddubsw_Pq_Qq },  // SSE4
  /* 66 */ { 0, &Ia_pmaddubsw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3805[4] = {
  /* -- */ { 0, &Ia_phsubw_Pq_Qq },     // SSE4
  /* 66 */ { 0, &Ia_phsubw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3806[4] = {
  /* -- */ { 0, &Ia_phsubd_Pq_Qq },     // SSE4
  /* 66 */ { 0, &Ia_phsubd_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3807[4] = {
  /* -- */ { 0, &Ia_phsubsw_Pq_Qq },    // SSE4
  /* 66 */ { 0, &Ia_phsubsw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3808[4] = {
  /* -- */ { 0, &Ia_psignb_Pq_Qq },     // SSE4
  /* 66 */ { 0, &Ia_psignb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3809[4] = {
  /* -- */ { 0, &Ia_psignw_Pq_Qq },     // SSE4
  /* 66 */ { 0, &Ia_psignw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f380a[4] = {
  /* -- */ { 0, &Ia_psignd_Pq_Qq },     // SSE4
  /* 66 */ { 0, &Ia_psignd_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f380b[4] = {
  /* -- */ { 0, &Ia_pmulhrsw_Pq_Qq },   // SSE4
  /* 66 */ { 0, &Ia_pmulhrsw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f381c[4] = {
  /* -- */ { 0, &Ia_pabsb_Pq_Qq },      // SSE4
  /* 66 */ { 0, &Ia_pabsb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f381d[4] = {
  /* -- */ { 0, &Ia_pabsw_Pq_Qq },      // SSE4
  /* 66 */ { 0, &Ia_pabsw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f381e[4] = {
  /* -- */ { 0, &Ia_pabsd_Pq_Qq },      // SSE4
  /* 66 */ { 0, &Ia_pabsd_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f3a0f[4] = {
  /* -- */ { 0, &Ia_palignr_Pq_Qq_Ib }, // SSE4
  /* 66 */ { 0, &Ia_palignr_Vdq_Wdq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f50[4] = {
  /* -- */ { 0, &Ia_movmskps_Gd_Vps },
  /* 66 */ { 0, &Ia_movmskpd_Gd_Vpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f51[4] = {
  /* -- */ { 0, &Ia_sqrtps_Vps_Wps },
  /* 66 */ { 0, &Ia_sqrtpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_sqrtsd_Vsd_Wsd },
  /* F3 */ { 0, &Ia_sqrtss_Vss_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f52[4] = {
  /* -- */ { 0, &Ia_rsqrtps_Vps_Wps },
  /* 66 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_rsqrtss_Vss_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f53[4] = {
  /* -- */ { 0, &Ia_rcpps_Vps_Wps },
  /* 66 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_rcpss_Vss_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f54[4] = {
  /* -- */ { 0, &Ia_andps_Vps_Wps },
  /* 66 */ { 0, &Ia_andpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f55[4] = {
  /* -- */ { 0, &Ia_andnps_Vps_Wps },
  /* 66 */ { 0, &Ia_andnpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f56[4] = {
  /* -- */ { 0, &Ia_orps_Vps_Wps },
  /* 66 */ { 0, &Ia_orpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f57[4] = {
  /* -- */ { 0, &Ia_xorps_Vps_Wps },
  /* 66 */ { 0, &Ia_xorpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f58[4] = {
  /* -- */ { 0, &Ia_addps_Vps_Wps },
  /* 66 */ { 0, &Ia_addpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_addsd_Vsd_Wsd },
  /* F3 */ { 0, &Ia_addss_Vss_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f59[4] = {
  /* -- */ { 0, &Ia_mulps_Vps_Wps },
  /* 66 */ { 0, &Ia_mulpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_mulsd_Vsd_Wsd },
  /* F3 */ { 0, &Ia_mulss_Vss_Wss }
};                   

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5a[4] = {
  /* -- */ { 0, &Ia_cvtps2pd_Vpd_Wps },
  /* 66 */ { 0, &Ia_cvtpd2ps_Vps_Wpd },
  /* F2 */ { 0, &Ia_cvtsd2ss_Vss_Wsd },
  /* F3 */ { 0, &Ia_cvtss2sd_Vsd_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5b[4] = {
  /* -- */ { 0, &Ia_cvtdq2ps_Vps_Wdq  },
  /* 66 */ { 0, &Ia_cvtps2dq_Vdq_Wps  },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_cvttps2dq_Vdq_Wps }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5c[4] = {
  /* -- */ { 0, &Ia_subps_Vps_Wps },
  /* 66 */ { 0, &Ia_subpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_subsd_Vsd_Wsd },
  /* F3 */ { 0, &Ia_subss_Vss_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5d[4] = {
  /* -- */ { 0, &Ia_minps_Vps_Wps },
  /* 66 */ { 0, &Ia_minpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_minsd_Vsd_Wsd },
  /* F3 */ { 0, &Ia_minss_Vss_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5e[4] = {
  /* -- */ { 0, &Ia_divps_Vps_Wps },
  /* 66 */ { 0, &Ia_divpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_divsd_Vsd_Wsd },
  /* F3 */ { 0, &Ia_divss_Vss_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f5f[4] = {
  /* -- */ { 0, &Ia_maxps_Vps_Wps },
  /* 66 */ { 0, &Ia_maxpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_maxsd_Vsd_Wsd },
  /* F3 */ { 0, &Ia_maxss_Vss_Wss }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f60[4] = {
  /* -- */ { 0, &Ia_punpcklbw_Pq_Qd },
  /* 66 */ { 0, &Ia_punpcklbw_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f61[4] = {
  /* -- */ { 0, &Ia_punpcklwd_Pq_Qd  },
  /* 66 */ { 0, &Ia_punpcklwd_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f62[4] = {
  /* -- */ { 0, &Ia_punpckldq_Pq_Qd  },
  /* 66 */ { 0, &Ia_punpckldq_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f63[4] = {
  /* -- */ { 0, &Ia_packsswb_Pq_Qq  },
  /* 66 */ { 0, &Ia_packsswb_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f64[4] = {
  /* -- */ { 0, &Ia_pcmpgtb_Pq_Qq  },
  /* 66 */ { 0, &Ia_pcmpgtb_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f65[4] = {
  /* -- */ { 0, &Ia_pcmpgtw_Pq_Qq  },
  /* 66 */ { 0, &Ia_pcmpgtw_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f66[4] = {
  /* -- */ { 0, &Ia_pcmpgtd_Pq_Qq   },
  /* 66 */ { 0, &Ia_pcmpgtd_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f67[4] = {
  /* -- */ { 0, &Ia_packuswb_Pq_Qq   },
  /* 66 */ { 0, &Ia_packuswb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f68[4] = {
  /* -- */ { 0, &Ia_punpckhbw_Pq_Qq  },
  /* 66 */ { 0, &Ia_punpckhbw_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f69[4] = {
  /* -- */ { 0, &Ia_punpckhwd_Pq_Qq  },
  /* 66 */ { 0, &Ia_punpckhwd_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6a[4] = {
  /* -- */ { 0, &Ia_punpckhdq_Pq_Qq  },
  /* 66 */ { 0, &Ia_punpckhdq_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6b[4] = {
  /* -- */ { 0, &Ia_packssdw_Pq_Qq   },
  /* 66 */ { 0, &Ia_packssdw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6c[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_punpcklqdq_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6d[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_punpckhqdq_Vdq_Wq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6e[4] = {
  /* -- */ { 0, &Ia_movd_Pq_Ed  },
  /* 66 */ { 0, &Ia_movd_Vdq_Ed },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6eQ[4] = {
  /* -- */ { 0, &Ia_movq_Pq_Eq  },
  /* 66 */ { 0, &Ia_movq_Vdq_Eq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f6f[4] = {
  /* -- */ { 0, &Ia_movq_Pq_Qq     },
  /* 66 */ { 0, &Ia_movdqa_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_movdqu_Vdq_Wdq },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f70[4] = {
  /* -- */ { 0, &Ia_pshufw_Pq_Qq_Ib   },
  /* 66 */ { 0, &Ia_pshufd_Vdq_Wdq_Ib },
  /* F2 */ { 0, &Ia_pshufhw_Vq_Wq_Ib  },
  /* F3 */ { 0, &Ia_pshuflw_Vq_Wq_Ib  }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f74[4] = {
  /* -- */ { 0, &Ia_pcmpeqb_Pq_Qq   },
  /* 66 */ { 0, &Ia_pcmpeqb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f75[4] = {
  /* -- */ { 0, &Ia_pcmpeqw_Pq_Qq   },
  /* 66 */ { 0, &Ia_pcmpeqw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f76[4] = {
  /* -- */ { 0, &Ia_pcmpeqd_Pq_Qq   },
  /* 66 */ { 0, &Ia_pcmpeqd_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f7c[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_haddpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_haddps_Vps_Wps },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f7d[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_hsubpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_hsubps_Vps_Wps },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f7e[4] = {
  /* -- */ { 0, &Ia_movd_Ed_Pq },
  /* 66 */ { 0, &Ia_movd_Ed_Vd },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_movq_Vq_Wq },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f7eQ[4] = {
  /* -- */ { 0, &Ia_movq_Eq_Pq },
  /* 66 */ { 0, &Ia_movq_Eq_Vq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_movq_Vq_Wq },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0f7f[4] = {
  /* -- */ { 0, &Ia_movq_Qq_Pq     },
  /* 66 */ { 0, &Ia_movdqa_Wdq_Vdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_movdqu_Wdq_Vdq },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fc2[4] = {
  /* -- */ { 0, &Ia_cmpps_Vps_Wps_Ib },
  /* 66 */ { 0, &Ia_cmppd_Vpd_Wpd_Ib },
  /* F2 */ { 0, &Ia_cmpsd_Vsd_Wsd_Ib },
  /* F3 */ { 0, &Ia_cmpss_Vss_Wss_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fc3[4] = {
  /* -- */ { 0, &Ia_movnti_Md_Gd },
  /* 66 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_640fc3[4] = {
  /* -- */ { 0, &Ia_movntiq_Mq_Gq },
  /* 66 */ { 0, &Ia_Invalid },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fc4[4] = {
  /* -- */ { 0, &Ia_pinsrw_Pq_Ed_Ib  },
  /* 66 */ { 0, &Ia_pinsrw_Vdq_Ed_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fc5[4] = {
  /* -- */ { 0, &Ia_pextrw_Gd_Nq_Ib  },
  /* 66 */ { 0, &Ia_pextrw_Gd_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fc6[4] = {
  /* -- */ { 0, &Ia_shufps_Vps_Wps_Ib },
  /* 66 */ { 0, &Ia_shufpd_Vpd_Wpd_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd0[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_addsubpd_Vpd_Wpd },
  /* F2 */ { 0, &Ia_addsubps_Vps_Wps },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd1[4] = {
  /* -- */ { 0, &Ia_psrlw_Pq_Qq   },
  /* 66 */ { 0, &Ia_psrlw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd2[4] = {
  /* -- */ { 0, &Ia_psrld_Pq_Qq   },
  /* 66 */ { 0, &Ia_psrld_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd3[4] = {
  /* -- */ { 0, &Ia_psrlq_Pq_Qq   },
  /* 66 */ { 0, &Ia_psrlq_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd4[4] = {
  /* -- */ { 0, &Ia_paddq_Pq_Qq   },
  /* 66 */ { 0, &Ia_paddq_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd5[4] = {
  /* -- */ { 0, &Ia_pmullw_Pq_Qq   },
  /* 66 */ { 0, &Ia_pmullw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd6[4] = {
  /* -- */ { 0, &Ia_Invalid        },
  /* 66 */ { 0, &Ia_movq_Wq_Vq     },
  /* F2 */ { 0, &Ia_movdq2q_Pq_Vq  },
  /* F3 */ { 0, &Ia_movq2dq_Vdq_Qq },
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd7[4] = {
  /* -- */ { 0, &Ia_pmovmskb_Gd_Nq  },
  /* 66 */ { 0, &Ia_pmovmskb_Gd_Udq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd8[4] = {
  /* -- */ { 0, &Ia_psubusb_Pq_Qq   },
  /* 66 */ { 0, &Ia_psubusb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fd9[4] = {
  /* -- */ { 0, &Ia_psubusw_Pq_Qq   },
  /* 66 */ { 0, &Ia_psubusw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fda[4] = {
  /* -- */ { 0, &Ia_pminub_Pq_Qq   },
  /* 66 */ { 0, &Ia_pminub_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fdb[4] = {
  /* -- */ { 0, &Ia_pand_Pq_Qq   },
  /* 66 */ { 0, &Ia_pand_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fdc[4] = {
  /* -- */ { 0, &Ia_paddusb_Pq_Qq   },
  /* 66 */ { 0, &Ia_paddusb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fdd[4] = {
  /* -- */ { 0, &Ia_paddusw_Pq_Qq   },
  /* 66 */ { 0, &Ia_paddusw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fde[4] = {
  /* -- */ { 0, &Ia_pmaxub_Pq_Qq   },
  /* 66 */ { 0, &Ia_pmaxub_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fdf[4] = {
  /* -- */ { 0, &Ia_pandn_Pq_Qq   },
  /* 66 */ { 0, &Ia_pandn_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe0[4] = {
  /* -- */ { 0, &Ia_pavgb_Pq_Qq   },
  /* 66 */ { 0, &Ia_pavgb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe1[4] = {
  /* -- */ { 0, &Ia_psraw_Pq_Qq   },
  /* 66 */ { 0, &Ia_psraw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe2[4] = {
  /* -- */ { 0, &Ia_psrad_Pq_Qq   },
  /* 66 */ { 0, &Ia_psrad_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe3[4] = {
  /* -- */ { 0, &Ia_pavgw_Pq_Qq   },
  /* 66 */ { 0, &Ia_pavgw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe4[4] = {
  /* -- */ { 0, &Ia_pmulhuw_Pq_Qq   },
  /* 66 */ { 0, &Ia_pmulhuw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe5[4] = {
  /* -- */ { 0, &Ia_pmulhw_Pq_Qq   },
  /* 66 */ { 0, &Ia_pmulhw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe6[4] = {
  /* -- */ { 0, &Ia_Invalid           },
  /* 66 */ { 0, &Ia_cvttpd2dq_Vq_Wpd },
  /* F2 */ { 0, &Ia_cvtpd2dq_Vq_Wpd   },
  /* F3 */ { 0, &Ia_cvtdq2pd_Vpd_Wq   }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe7[4] = {
  /* -- */ { 0, &Ia_movntq_Mq_Pq    },
  /* 66 */ { 0, &Ia_movntdq_Mdq_Vdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe8[4] = {
  /* -- */ { 0, &Ia_psubsb_Pq_Qq   },
  /* 66 */ { 0, &Ia_psubsb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fe9[4] = {
  /* -- */ { 0, &Ia_psubsw_Pq_Qq   },
  /* 66 */ { 0, &Ia_psubsw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fea[4] = {
  /* -- */ { 0, &Ia_pminsw_Pq_Qq   },
  /* 66 */ { 0, &Ia_pminsw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0feb[4] = {
  /* -- */ { 0, &Ia_por_Pq_Qq   },
  /* 66 */ { 0, &Ia_por_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fec[4] = {
  /* -- */ { 0, &Ia_paddsb_Pq_Qq   },
  /* 66 */ { 0, &Ia_paddsb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fed[4] = {
  /* -- */ { 0, &Ia_paddsw_Pq_Qq   },
  /* 66 */ { 0, &Ia_paddsw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fee[4] = {
  /* -- */ { 0, &Ia_pmaxuw_Pq_Qq   },
  /* 66 */ { 0, &Ia_pmaxuw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0fef[4] = {
  /* -- */ { 0, &Ia_pxor_Pq_Qq   },
  /* 66 */ { 0, &Ia_pxor_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff0[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_lddqu_Vdq_Mdq },
  /* F3 */ { 0, &Ia_Invalid }
};                        

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff1[4] = {
  /* -- */ { 0, &Ia_psllw_Pq_Qq   },
  /* 66 */ { 0, &Ia_psllw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff2[4] = {
  /* -- */ { 0, &Ia_pslld_Pq_Qq   },
  /* 66 */ { 0, &Ia_pslld_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff3[4] = {
  /* -- */ { 0, &Ia_psllq_Pq_Qq   },
  /* 66 */ { 0, &Ia_psllq_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff4[4] = {
  /* -- */ { 0, &Ia_pmuludq_Pq_Qq   },
  /* 66 */ { 0, &Ia_pmuludq_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff5[4] = {
  /* -- */ { 0, &Ia_pmaddwd_Pq_Qq   },
  /* 66 */ { 0, &Ia_pmaddwd_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff6[4] = {
  /* -- */ { 0, &Ia_psadbw_Pq_Qq   },
  /* 66 */ { 0, &Ia_psadbw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff7[4] = {
  /* -- */ { 0, &Ia_maskmovq_Pq_Nq     },
  /* 66 */ { 0, &Ia_maskmovdqu_Vdq_Udq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff8[4] = {
  /* -- */ { 0, &Ia_psubb_Pq_Qq   },
  /* 66 */ { 0, &Ia_psubb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ff9[4] = {
  /* -- */ { 0, &Ia_psubw_Pq_Qq   },
  /* 66 */ { 0, &Ia_psubw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ffa[4] = {
  /* -- */ { 0, &Ia_psubd_Pq_Qq   },
  /* 66 */ { 0, &Ia_psubd_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ffb[4] = {
  /* -- */ { 0, &Ia_psubq_Pq_Qq   },
  /* 66 */ { 0, &Ia_psubq_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ffc[4] = {
  /* -- */ { 0, &Ia_paddb_Pq_Qq   },
  /* 66 */ { 0, &Ia_paddb_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ffd[4] = {
  /* -- */ { 0, &Ia_paddw_Pq_Qq   },
  /* 66 */ { 0, &Ia_paddw_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_0ffe[4] = {
  /* -- */ { 0, &Ia_paddd_Pq_Qq   },
  /* 66 */ { 0, &Ia_paddd_Vdq_Wdq },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1202[4] = {
  /* -- */ { 0, &Ia_psrlw_Nq_Ib  },
  /* 66 */ { 0, &Ia_psrlw_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1204[4] = {
  /* -- */ { 0, &Ia_psraw_Nq_Ib  },
  /* 66 */ { 0, &Ia_psraw_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1206[4] = {
  /* -- */ { 0, &Ia_psllw_Nq_Ib  },
  /* 66 */ { 0, &Ia_psllw_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1302[4] = {
  /* -- */ { 0, &Ia_psrld_Nq_Ib  },
  /* 66 */ { 0, &Ia_psrld_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1304[4] = {
  /* -- */ { 0, &Ia_psrad_Nq_Ib  },
  /* 66 */ { 0, &Ia_psrad_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1306[4] = {
  /* -- */ { 0, &Ia_pslld_Nq_Ib  },
  /* 66 */ { 0, &Ia_pslld_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1402[4] = {
  /* -- */ { 0, &Ia_psrlq_Nq_Ib  },
  /* 66 */ { 0, &Ia_psrlq_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1403[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_psrldq_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1406[4] = {
  /* -- */ { 0, &Ia_psllq_Nq_Ib  },
  /* 66 */ { 0, &Ia_psllq_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupSSE_G1407[4] = {
  /* -- */ { 0, &Ia_Invalid },
  /* 66 */ { 0, &Ia_pslldq_Udq_Ib },
  /* F2 */ { 0, &Ia_Invalid },
  /* F3 */ { 0, &Ia_Invalid }
};


/* ************************************************************************ */
/* Opcode GroupN */

static BxDisasmOpcodeTable_t BxDisasmGroupG1EbIb[8] = {
  /* 0 */ { 0, &Ia_addb_Eb_Ib },
  /* 1 */ { 0, &Ia_orb_Eb_Ib  },
  /* 2 */ { 0, &Ia_adcb_Eb_Ib },
  /* 3 */ { 0, &Ia_sbbb_Eb_Ib },
  /* 4 */ { 0, &Ia_andb_Eb_Ib },
  /* 5 */ { 0, &Ia_subb_Eb_Ib },
  /* 6 */ { 0, &Ia_xorb_Eb_Ib },
  /* 7 */ { 0, &Ia_cmpb_Eb_Ib }
};

static BxDisasmOpcodeTable_t BxDisasmGroupG1EwIw[8] = {
  /* 0 */ { 0, &Ia_addw_Ew_Iw },
  /* 1 */ { 0, &Ia_orw_Ew_Iw  },
  /* 2 */ { 0, &Ia_adcw_Ew_Iw },
  /* 3 */ { 0, &Ia_sbbw_Ew_Iw },
  /* 4 */ { 0, &Ia_andw_Ew_Iw },
  /* 5 */ { 0, &Ia_subw_Ew_Iw },
  /* 6 */ { 0, &Ia_xorw_Ew_Iw },
  /* 7 */ { 0, &Ia_cmpw_Ew_Iw }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG1EdId[8] = {
  /* 0 */ { 0, &Ia_addl_Ed_Id },
  /* 1 */ { 0, &Ia_orl_Ed_Id  },
  /* 2 */ { 0, &Ia_adcl_Ed_Id },
  /* 3 */ { 0, &Ia_sbbl_Ed_Id },
  /* 4 */ { 0, &Ia_andl_Ed_Id },
  /* 5 */ { 0, &Ia_subl_Ed_Id },
  /* 6 */ { 0, &Ia_xorl_Ed_Id },
  /* 7 */ { 0, &Ia_cmpl_Ed_Id }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG1EqId[8] = {
  /* 0 */ { 0, &Ia_addq_Eq_sId },
  /* 1 */ { 0, &Ia_orq_Eq_sId  },
  /* 2 */ { 0, &Ia_adcq_Eq_sId },
  /* 3 */ { 0, &Ia_sbbq_Eq_sId },
  /* 4 */ { 0, &Ia_andq_Eq_sId },
  /* 5 */ { 0, &Ia_subq_Eq_sId },
  /* 6 */ { 0, &Ia_xorq_Eq_sId },
  /* 7 */ { 0, &Ia_cmpq_Eq_sId }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG1EwIb[8] = {
  /* 0 */ { 0, &Ia_addw_Ew_sIb },	// sign-extend byte 
  /* 1 */ { 0, &Ia_orw_Ew_sIb  },
  /* 2 */ { 0, &Ia_adcw_Ew_sIb },
  /* 3 */ { 0, &Ia_sbbw_Ew_sIb },
  /* 4 */ { 0, &Ia_andw_Ew_sIb },
  /* 5 */ { 0, &Ia_subw_Ew_sIb },
  /* 6 */ { 0, &Ia_xorw_Ew_sIb },
  /* 7 */ { 0, &Ia_cmpw_Ew_sIb }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG1EdIb[8] = {
  /* 0 */ { 0, &Ia_addl_Ed_sIb },	// sign-extend byte 
  /* 1 */ { 0, &Ia_orl_Ed_sIb  },
  /* 2 */ { 0, &Ia_adcl_Ed_sIb },
  /* 3 */ { 0, &Ia_sbbl_Ed_sIb },
  /* 4 */ { 0, &Ia_andl_Ed_sIb },
  /* 5 */ { 0, &Ia_subl_Ed_sIb },
  /* 6 */ { 0, &Ia_xorl_Ed_sIb },
  /* 7 */ { 0, &Ia_cmpl_Ed_sIb }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG1EqIb[8] = {
  /* 0 */ { 0, &Ia_addq_Eq_sIb },	// sign-extend byte 
  /* 1 */ { 0, &Ia_orq_Eq_sIb  },
  /* 2 */ { 0, &Ia_adcq_Eq_sIb },
  /* 3 */ { 0, &Ia_sbbq_Eq_sIb },
  /* 4 */ { 0, &Ia_andq_Eq_sIb },
  /* 5 */ { 0, &Ia_subq_Eq_sIb },
  /* 6 */ { 0, &Ia_xorq_Eq_sIb },
  /* 7 */ { 0, &Ia_cmpq_Eq_sIb }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2Eb[8] = {
  /* 0 */ { 0, &Ia_rolb_Eb_Ib },
  /* 1 */ { 0, &Ia_rorb_Eb_Ib },
  /* 2 */ { 0, &Ia_rclb_Eb_Ib },
  /* 3 */ { 0, &Ia_rcrb_Eb_Ib },
  /* 4 */ { 0, &Ia_shlb_Eb_Ib },
  /* 5 */ { 0, &Ia_shrb_Eb_Ib },
  /* 6 */ { 0, &Ia_shlb_Eb_Ib },
  /* 7 */ { 0, &Ia_sarb_Eb_Ib }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2EbI1[8] = {
  /* 0 */ { 0, &Ia_rolb_Eb_I1 },
  /* 1 */ { 0, &Ia_rorb_Eb_I1 },
  /* 2 */ { 0, &Ia_rclb_Eb_I1 },
  /* 3 */ { 0, &Ia_rcrb_Eb_I1 },
  /* 4 */ { 0, &Ia_shlb_Eb_I1 },
  /* 5 */ { 0, &Ia_shrb_Eb_I1 },
  /* 6 */ { 0, &Ia_shlb_Eb_I1 },
  /* 7 */ { 0, &Ia_sarb_Eb_I1 }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2EbCL[8] = {
  /* 0 */ { 0, &Ia_rolb_Eb_CL },
  /* 1 */ { 0, &Ia_rorb_Eb_CL },
  /* 2 */ { 0, &Ia_rclb_Eb_CL },
  /* 3 */ { 0, &Ia_rcrb_Eb_CL },
  /* 4 */ { 0, &Ia_shlb_Eb_CL },
  /* 5 */ { 0, &Ia_shrb_Eb_CL },
  /* 6 */ { 0, &Ia_shlb_Eb_CL },
  /* 7 */ { 0, &Ia_sarb_Eb_CL }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2Ew[8] = {
  /* 0 */ { 0, &Ia_rolw_Ew_Ib },
  /* 1 */ { 0, &Ia_rorw_Ew_Ib },
  /* 2 */ { 0, &Ia_rclw_Ew_Ib },
  /* 3 */ { 0, &Ia_rcrw_Ew_Ib },
  /* 4 */ { 0, &Ia_shlw_Ew_Ib },
  /* 5 */ { 0, &Ia_shrw_Ew_Ib },
  /* 6 */ { 0, &Ia_shlw_Ew_Ib },
  /* 7 */ { 0, &Ia_sarw_Ew_Ib }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2Ed[8] = {
  /* 0 */ { 0, &Ia_roll_Ed_Ib },
  /* 1 */ { 0, &Ia_rorl_Ed_Ib },
  /* 2 */ { 0, &Ia_rcll_Ed_Ib },
  /* 3 */ { 0, &Ia_rcrl_Ed_Ib },
  /* 4 */ { 0, &Ia_shll_Ed_Ib },
  /* 5 */ { 0, &Ia_shrl_Ed_Ib },
  /* 6 */ { 0, &Ia_shll_Ed_Ib },
  /* 7 */ { 0, &Ia_sarl_Ed_Ib }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2Eq[8] = {
  /* 0 */ { 0, &Ia_rolq_Eq_Ib },
  /* 1 */ { 0, &Ia_rorq_Eq_Ib },
  /* 2 */ { 0, &Ia_rclq_Eq_Ib },
  /* 3 */ { 0, &Ia_rcrq_Eq_Ib },
  /* 4 */ { 0, &Ia_shlq_Eq_Ib },
  /* 5 */ { 0, &Ia_shrq_Eq_Ib },
  /* 6 */ { 0, &Ia_shlq_Eq_Ib },
  /* 7 */ { 0, &Ia_sarq_Eq_Ib }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2EwI1[8] = {
  /* 0 */ { 0, &Ia_rolw_Ew_I1 },
  /* 1 */ { 0, &Ia_rorw_Ew_I1 },
  /* 2 */ { 0, &Ia_rclw_Ew_I1 },
  /* 3 */ { 0, &Ia_rcrw_Ew_I1 },
  /* 4 */ { 0, &Ia_shlw_Ew_I1 },
  /* 5 */ { 0, &Ia_shrw_Ew_I1 },
  /* 6 */ { 0, &Ia_shlw_Ew_I1 },
  /* 7 */ { 0, &Ia_sarw_Ew_I1 }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2EdI1[8] = {
  /* 0 */ { 0, &Ia_roll_Ed_I1 },
  /* 1 */ { 0, &Ia_rorl_Ed_I1 },
  /* 2 */ { 0, &Ia_rcll_Ed_I1 },
  /* 3 */ { 0, &Ia_rcrl_Ed_I1 },
  /* 4 */ { 0, &Ia_shll_Ed_I1 },
  /* 5 */ { 0, &Ia_shrl_Ed_I1 },
  /* 6 */ { 0, &Ia_shll_Ed_I1 },
  /* 7 */ { 0, &Ia_sarl_Ed_I1 }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2EqI1[8] = {
  /* 0 */ { 0, &Ia_rolq_Eq_I1 },
  /* 1 */ { 0, &Ia_rorq_Eq_I1 },
  /* 2 */ { 0, &Ia_rclq_Eq_I1 },
  /* 3 */ { 0, &Ia_rcrq_Eq_I1 },
  /* 4 */ { 0, &Ia_shlq_Eq_I1 },
  /* 5 */ { 0, &Ia_shrq_Eq_I1 },
  /* 6 */ { 0, &Ia_shlq_Eq_I1 },
  /* 7 */ { 0, &Ia_sarq_Eq_I1 }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2EwCL[8] = {
  /* 0 */ { 0, &Ia_rolw_Ew_CL },
  /* 1 */ { 0, &Ia_rorw_Ew_CL },
  /* 2 */ { 0, &Ia_rclw_Ew_CL },
  /* 3 */ { 0, &Ia_rcrw_Ew_CL },
  /* 4 */ { 0, &Ia_shlw_Ew_CL },
  /* 5 */ { 0, &Ia_shrw_Ew_CL },
  /* 6 */ { 0, &Ia_shlw_Ew_CL },
  /* 7 */ { 0, &Ia_sarw_Ew_CL }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2EdCL[8] = {
  /* 0 */ { 0, &Ia_roll_Ed_CL },
  /* 1 */ { 0, &Ia_rorl_Ed_CL },
  /* 2 */ { 0, &Ia_rcll_Ed_CL },
  /* 3 */ { 0, &Ia_rcrl_Ed_CL },
  /* 4 */ { 0, &Ia_shll_Ed_CL },
  /* 5 */ { 0, &Ia_shrl_Ed_CL },
  /* 6 */ { 0, &Ia_shll_Ed_CL },
  /* 7 */ { 0, &Ia_sarl_Ed_CL }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG2EqCL[8] = {
  /* 0 */ { 0, &Ia_rolq_Eq_CL },
  /* 1 */ { 0, &Ia_rorq_Eq_CL },
  /* 2 */ { 0, &Ia_rclq_Eq_CL },
  /* 3 */ { 0, &Ia_rcrq_Eq_CL },
  /* 4 */ { 0, &Ia_shlq_Eq_CL },
  /* 5 */ { 0, &Ia_shrq_Eq_CL },
  /* 6 */ { 0, &Ia_shlq_Eq_CL },
  /* 7 */ { 0, &Ia_sarq_Eq_CL }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG3Eb[8] = {
  /* 0 */ { 0, &Ia_testb_Eb_Ib },
  /* 1 */ { 0, &Ia_testb_Eb_Ib },
  /* 2 */ { 0, &Ia_notb_Eb     },
  /* 3 */ { 0, &Ia_negb_Eb     },
  /* 4 */ { 0, &Ia_mulb_AL_Eb  },
  /* 5 */ { 0, &Ia_imulb_AL_Eb },
  /* 6 */ { 0, &Ia_divb_AL_Eb  },
  /* 7 */ { 0, &Ia_idivb_AL_Eb }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG3Ew[8] = {
  /* 0 */ { 0, &Ia_testw_Ew_Iw },
  /* 1 */ { 0, &Ia_testw_Ew_Iw },
  /* 2 */ { 0, &Ia_notw_Ew     },
  /* 3 */ { 0, &Ia_negw_Ew     },
  /* 4 */ { 0, &Ia_mulw_AX_Ew  },
  /* 5 */ { 0, &Ia_imulw_AX_Ew },
  /* 6 */ { 0, &Ia_divw_AX_Ew  },
  /* 7 */ { 0, &Ia_idivw_AX_Ew }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG3Ed[8] = {
  /* 0 */ { 0, &Ia_testl_Ed_Id  },
  /* 1 */ { 0, &Ia_testl_Ed_Id  },
  /* 2 */ { 0, &Ia_notl_Ed      },
  /* 3 */ { 0, &Ia_negl_Ed      },
  /* 4 */ { 0, &Ia_mull_EAX_Ed  },
  /* 5 */ { 0, &Ia_imull_EAX_Ed },
  /* 6 */ { 0, &Ia_divl_EAX_Ed  },
  /* 7 */ { 0, &Ia_idivl_EAX_Ed }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG3Eq[8] = {
  /* 0 */ { 0, &Ia_testq_Eq_sId },
  /* 1 */ { 0, &Ia_testq_Eq_sId },
  /* 2 */ { 0, &Ia_notq_Eq      },
  /* 3 */ { 0, &Ia_negq_Eq      },
  /* 4 */ { 0, &Ia_mulq_RAX_Eq  },
  /* 5 */ { 0, &Ia_imulq_RAX_Eq },
  /* 6 */ { 0, &Ia_divq_RAX_Eq  },
  /* 7 */ { 0, &Ia_idivq_RAX_Eq }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG4[8] = {
  /* 0 */ { 0, &Ia_incb_Eb },
  /* 1 */ { 0, &Ia_decb_Eb },
  /* 2 */ { 0, &Ia_Invalid },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { 0, &Ia_Invalid },
  /* 7 */ { 0, &Ia_Invalid }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG5w[8] = {
  /* 0 */ { 0, &Ia_incw_Ew  },
  /* 1 */ { 0, &Ia_decw_Ew  },
  /* 2 */ { 0, &Ia_call_Ew  },
  /* 3 */ { 0, &Ia_lcall_Mp },
  /* 4 */ { 0, &Ia_jmp_Ew   },
  /* 5 */ { 0, &Ia_ljmp_Mp  },
  /* 6 */ { 0, &Ia_pushw_Ew },
  /* 7 */ { 0, &Ia_Invalid  }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG5d[8] = {
  /* 0 */ { 0, &Ia_incl_Ed  },
  /* 1 */ { 0, &Ia_decl_Ed  },
  /* 2 */ { 0, &Ia_call_Ed  },
  /* 3 */ { 0, &Ia_lcall_Mp },
  /* 4 */ { 0, &Ia_jmp_Ed   },
  /* 5 */ { 0, &Ia_ljmp_Mp  },
  /* 6 */ { 0, &Ia_pushl_Ed },
  /* 7 */ { 0, &Ia_Invalid  }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroup64G5d[8] = {
  /* 0 */ { 0, &Ia_incl_Ed  },
  /* 1 */ { 0, &Ia_decl_Ed  },
  /* 2 */ { 0, &Ia_call_Eq  },
  /* 3 */ { 0, &Ia_lcall_Mp },
  /* 4 */ { 0, &Ia_jmp_Eq   },
  /* 5 */ { 0, &Ia_ljmp_Mp  },
  /* 6 */ { 0, &Ia_pushq_Eq },
  /* 7 */ { 0, &Ia_Invalid  }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroup64G5q[8] = {
  /* 0 */ { 0, &Ia_incq_Eq  },
  /* 1 */ { 0, &Ia_decq_Eq  },
  /* 2 */ { 0, &Ia_call_Eq  },
  /* 3 */ { 0, &Ia_lcall_Mp },
  /* 4 */ { 0, &Ia_jmp_Eq   },
  /* 5 */ { 0, &Ia_ljmp_Mp  },
  /* 6 */ { 0, &Ia_pushq_Eq },
  /* 7 */ { 0, &Ia_Invalid  }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG6[8] = {
  /* 0 */ { 0, &Ia_sldt    },
  /* 1 */ { 0, &Ia_str     },
  /* 2 */ { 0, &Ia_lldt    },
  /* 3 */ { 0, &Ia_ltr     },
  /* 4 */ { 0, &Ia_verr    },
  /* 5 */ { 0, &Ia_verw    },
  /* 6 */ { 0, &Ia_Invalid },
  /* 7 */ { 0, &Ia_Invalid }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupRmINVLPG[8] = {
  /* 0 */ { 0, &Ia_swapgs  },
  /* 1 */ { 0, &Ia_rdtscp  },
  /* 2 */ { 0, &Ia_Invalid },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { 0, &Ia_Invalid },
  /* 7 */ { 0, &Ia_Invalid },
};

static BxDisasmOpcodeTable_t BxDisasmGroupModINVLPG[2] = {
  /* R */ { GRPRM(INVLPG) },
  /* M */ { 0, &Ia_invlpg }
};

static BxDisasmOpcodeTable_t BxDisasmGroupRmSIDT[8] = {
  /* 0 */ { 0, &Ia_monitor },
  /* 1 */ { 0, &Ia_mwait   },
  /* 2 */ { 0, &Ia_Invalid },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { 0, &Ia_Invalid },
  /* 7 */ { 0, &Ia_Invalid },
};

static BxDisasmOpcodeTable_t BxDisasmGroupModSIDT[2] = {
  /* R */ { GRPRM(SIDT)   },
  /* M */ { 0, &Ia_sidt   }
};

static BxDisasmOpcodeTable_t BxDisasmGroupG7[8] = {
  /* 0 */ { 0, &Ia_sgdt    },
  /* 1 */ { GRPMOD(SIDT)   },
  /* 2 */ { 0, &Ia_lgdt    },
  /* 3 */ { 0, &Ia_lidt    },
  /* 4 */ { 0, &Ia_smsw_Ew },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { 0, &Ia_lmsw_Ew },
  /* 7 */ { GRPMOD(INVLPG) },
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG8EwIb[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { 0, &Ia_Invalid },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { 0, &Ia_btw_Ew_Ib  },
  /* 5 */ { 0, &Ia_btsw_Ew_Ib },
  /* 6 */ { 0, &Ia_btrw_Ew_Ib },
  /* 7 */ { 0, &Ia_btcw_Ew_Ib }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG8EdIb[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { 0, &Ia_Invalid },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { 0, &Ia_btl_Ed_Ib  },
  /* 5 */ { 0, &Ia_btsl_Ed_Ib },
  /* 6 */ { 0, &Ia_btrl_Ed_Ib },
  /* 7 */ { 0, &Ia_btcl_Ed_Ib }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG8EqIb[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { 0, &Ia_Invalid },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { 0, &Ia_btq_Eq_Ib  },
  /* 5 */ { 0, &Ia_btsq_Eq_Ib },
  /* 6 */ { 0, &Ia_btrq_Eq_Ib },
  /* 7 */ { 0, &Ia_btcq_Eq_Ib }
}; 

static BxDisasmOpcodeTable_t BxDisasmGroupG9[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_cmpxchg8b },
  /* 2 */ { 0, &Ia_Invalid },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { 0, &Ia_Invalid },
  /* 7 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupG9q[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_cmpxchg16b },
  /* 2 */ { 0, &Ia_Invalid },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { 0, &Ia_Invalid },
  /* 7 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupG12[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { GRPSSE(G1202)  },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { GRPSSE(G1204)  },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { GRPSSE(G1206)  },
  /* 7 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupG13[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { GRPSSE(G1302)  },
  /* 3 */ { 0, &Ia_Invalid },
  /* 4 */ { GRPSSE(G1304)  },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { GRPSSE(G1306)  },
  /* 7 */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmGroupG14[8] = {
  /* 0 */ { 0, &Ia_Invalid },
  /* 1 */ { 0, &Ia_Invalid },
  /* 2 */ { GRPSSE(G1402)  },
  /* 3 */ { GRPSSE(G1403)  },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { GRPSSE(G1406)  },
  /* 7 */ { GRPSSE(G1407)  } 
};

static BxDisasmOpcodeTable_t BxDisasmGroupModCFLUSH[2] = {
  /* R */ { 0, &Ia_sfence }, 
  /* M */ { 0, &Ia_cflush }
};

static BxDisasmOpcodeTable_t BxDisasmGroupG15[8] = {
  /* 0 */ { 0, &Ia_fxsave  },
  /* 1 */ { 0, &Ia_fxrstor },
  /* 2 */ { 0, &Ia_ldmxcsr },
  /* 3 */ { 0, &Ia_stmxcsr },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_lfence  },
  /* 6 */ { 0, &Ia_mfence  },
  /* 7 */ { GRPMOD(CFLUSH) }       /* SFENCE/CFLUSH */
};

static BxDisasmOpcodeTable_t BxDisasmGroupG16[8] =
{
  /* 0 */ { 0, &Ia_prefetchnta },
  /* 1 */ { 0, &Ia_prefetcht0  },
  /* 2 */ { 0, &Ia_prefetcht1  },
  /* 3 */ { 0, &Ia_prefetcht2  },
  /* 4 */ { 0, &Ia_Invalid },
  /* 5 */ { 0, &Ia_Invalid },
  /* 6 */ { 0, &Ia_Invalid },
  /* 7 */ { 0, &Ia_Invalid }
};

/* ************************************************************************ */
/* 3DNow! opcodes */

static BxDisasmOpcodeTable_t BxDisasm3DNowGroup[256] = {
  // 256 entries for 3DNow opcodes_by suffix
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
  /* 0C */ { 0, &Ia_pi2fw_Pq_Qq },
  /* 0D */ { 0, &Ia_pi2fd_Pq_Qq },
  /* 0E */ { 0, &Ia_Invalid },
  /* 0F */ { 0, &Ia_Invalid },
  /* 10 */ { 0, &Ia_Invalid },
  /* 11 */ { 0, &Ia_Invalid },
  /* 12 */ { 0, &Ia_Invalid },
  /* 13 */ { 0, &Ia_Invalid },
  /* 14 */ { 0, &Ia_Invalid },
  /* 15 */ { 0, &Ia_Invalid },
  /* 16 */ { 0, &Ia_Invalid },
  /* 17 */ { 0, &Ia_Invalid },
  /* 18 */ { 0, &Ia_Invalid },
  /* 19 */ { 0, &Ia_Invalid },
  /* 1A */ { 0, &Ia_Invalid },
  /* 1B */ { 0, &Ia_Invalid },
  /* 1C */ { 0, &Ia_pf2iw_Pq_Qq },
  /* 1D */ { 0, &Ia_pf2id_Pq_Qq },
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
  /* 8A */ { 0, &Ia_pfnacc_Pq_Qq },
  /* 8B */ { 0, &Ia_Invalid },
  /* 8C */ { 0, &Ia_Invalid },
  /* 8D */ { 0, &Ia_Invalid },
  /* 8E */ { 0, &Ia_pfpnacc_Pq_Qq },
  /* 8F */ { 0, &Ia_Invalid },
  /* 90 */ { 0, &Ia_pfcmpge_Pq_Qq },
  /* 91 */ { 0, &Ia_Invalid },
  /* 92 */ { 0, &Ia_Invalid },
  /* 93 */ { 0, &Ia_Invalid },
  /* 94 */ { 0, &Ia_pfmin_Pq_Qq },
  /* 95 */ { 0, &Ia_Invalid },
  /* 96 */ { 0, &Ia_pfrcp_Pq_Qq },
  /* 97 */ { 0, &Ia_pfrsqrt_Pq_Qq },
  /* 98 */ { 0, &Ia_Invalid },
  /* 99 */ { 0, &Ia_Invalid },
  /* 9A */ { 0, &Ia_pfsub_Pq_Qq },
  /* 9B */ { 0, &Ia_Invalid },
  /* 9C */ { 0, &Ia_Invalid },
  /* 9D */ { 0, &Ia_Invalid },
  /* 9E */ { 0, &Ia_pfadd_Pq_Qq },
  /* 9F */ { 0, &Ia_Invalid },
  /* A0 */ { 0, &Ia_pfcmpgt_Pq_Qq },
  /* A1 */ { 0, &Ia_Invalid },
  /* A2 */ { 0, &Ia_Invalid },
  /* A3 */ { 0, &Ia_Invalid },
  /* A4 */ { 0, &Ia_pfmax_Pq_Qq },
  /* A5 */ { 0, &Ia_Invalid },
  /* A6 */ { 0, &Ia_pfrcpit1_Pq_Qq },
  /* A7 */ { 0, &Ia_pfrsqit1_Pq_Qq },
  /* A8 */ { 0, &Ia_Invalid },
  /* A9 */ { 0, &Ia_Invalid },
  /* AA */ { 0, &Ia_pfsubr_Pq_Qq },
  /* AB */ { 0, &Ia_Invalid },
  /* AC */ { 0, &Ia_Invalid },
  /* AD */ { 0, &Ia_Invalid },
  /* AE */ { 0, &Ia_pfacc_Pq_Qq },
  /* AF */ { 0, &Ia_Invalid },
  /* B0 */ { 0, &Ia_pfcmpeq_Pq_Qq },
  /* B1 */ { 0, &Ia_Invalid },
  /* B2 */ { 0, &Ia_Invalid },
  /* B3 */ { 0, &Ia_Invalid },
  /* B4 */ { 0, &Ia_pfmul_Pq_Qq },
  /* B5 */ { 0, &Ia_Invalid },
  /* B6 */ { 0, &Ia_pfrcpit2_Pq_Qq },
  /* B7 */ { 0, &Ia_pmulhrw_Pq_Qq },
  /* B8 */ { 0, &Ia_Invalid },
  /* B9 */ { 0, &Ia_Invalid },
  /* BA */ { 0, &Ia_Invalid },
  /* BB */ { 0, &Ia_pswapd_Pq_Qq },
  /* BC */ { 0, &Ia_Invalid },
  /* BD */ { 0, &Ia_Invalid },
  /* BE */ { 0, &Ia_Invalid },
  /* BF */ { 0, &Ia_pavgb_Pq_Qq },
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
  /* DF */ { 0, &Ia_Invalid },
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

/* ************************************************************************ */
/* FPU Opcodes */

// floating point instructions when mod!=11b.
// the following tables will be accessed like groups using the nnn (reg) field of
// the modrm byte. (the first byte is D8-DF)

  // D8 (modrm is outside 00h - BFh) (mod != 11)
static BxDisasmOpcodeTable_t BxDisasmFPGroupD8[8] = { 
  /* 0 */ { 0, &Ia_fadds_Md  },
  /* 1 */ { 0, &Ia_fmuls_Md  },
  /* 2 */ { 0, &Ia_fcoms_Md  },
  /* 3 */ { 0, &Ia_fcomps_Md },
  /* 4 */ { 0, &Ia_fsubs_Md  },
  /* 5 */ { 0, &Ia_fsubrs_Md },
  /* 6 */ { 0, &Ia_fdivs_Md  },
  /* 7 */ { 0, &Ia_fdivrs_Md }
};

  // D9 (modrm is outside 00h - BFh) (mod != 11)
static BxDisasmOpcodeTable_t BxDisasmFPGroupD9[8] = { 
  /* 0 */ { 0, &Ia_flds_Md   },
  /* 1 */ { 0, &Ia_Invalid   },
  /* 2 */ { 0, &Ia_fsts_Md   },
  /* 3 */ { 0, &Ia_fstps_Md  },
  /* 4 */ { 0, &Ia_fldenv    },
  /* 5 */ { 0, &Ia_fldcw     },
  /* 6 */ { 0, &Ia_fnstenv   },
  /* 7 */ { 0, &Ia_fnstcw    }
};

  // DA (modrm is outside 00h - BFh) (mod != 11)
static BxDisasmOpcodeTable_t BxDisasmFPGroupDA[8] = { 
  /* 0 */ { 0, &Ia_fiaddl_Md  },
  /* 1 */ { 0, &Ia_fimull_Md  },
  /* 2 */ { 0, &Ia_ficoml_Md  },
  /* 3 */ { 0, &Ia_ficompl_Md },
  /* 4 */ { 0, &Ia_fisubl_Md  },
  /* 5 */ { 0, &Ia_fisubrl_Md },
  /* 6 */ { 0, &Ia_fidivl_Md  },
  /* 7 */ { 0, &Ia_fidivrl_Md }
};

  // DB (modrm is outside 00h - BFh) (mod != 11)
static BxDisasmOpcodeTable_t BxDisasmFPGroupDB[8] = { 
  /* 0 */ { 0, &Ia_fildl_Md   },
  /* 1 */ { 0, &Ia_fisttpl_Md },
  /* 2 */ { 0, &Ia_fistl_Md   },
  /* 3 */ { 0, &Ia_fistpl_Md  },
  /* 4 */ { 0, &Ia_Invalid    },
  /* 5 */ { 0, &Ia_fldt_Mt    },
  /* 6 */ { 0, &Ia_Invalid    },
  /* 7 */ { 0, &Ia_fstpt_Mt   }
};

  // DC (modrm is outside 00h - BFh) (mod != 11)
static BxDisasmOpcodeTable_t BxDisasmFPGroupDC[8] = { 
  /* 0 */ { 0, &Ia_faddl_Mq  },
  /* 1 */ { 0, &Ia_fmull_Mq  },
  /* 2 */ { 0, &Ia_fcoml_Mq  },
  /* 3 */ { 0, &Ia_fcompl_Mq },
  /* 4 */ { 0, &Ia_fsubl_Mq  },
  /* 5 */ { 0, &Ia_fsubrl_Mq },
  /* 6 */ { 0, &Ia_fdivl_Mq  },
  /* 7 */ { 0, &Ia_fdivrl_Mq }
};

  // DD (modrm is outside 00h - BFh) (mod != 11)
static BxDisasmOpcodeTable_t BxDisasmFPGroupDD[8] = { 
  /* 0 */ { 0, &Ia_fldl_Mq    },
  /* 1 */ { 0, &Ia_fisttpq_Mq },
  /* 2 */ { 0, &Ia_fstl_Mq    },
  /* 3 */ { 0, &Ia_fstpl_Mq   },
  /* 4 */ { 0, &Ia_frstor     },
  /* 5 */ { 0, &Ia_Invalid    },
  /* 6 */ { 0, &Ia_fnsave     },
  /* 7 */ { 0, &Ia_fnstsw     }
};

  // DE (modrm is outside 00h - BFh) (mod != 11)
static BxDisasmOpcodeTable_t BxDisasmFPGroupDE[8] = { 
  /* 0 */ { 0, &Ia_fiadds_Mw  },
  /* 1 */ { 0, &Ia_fimuls_Mw  },
  /* 2 */ { 0, &Ia_ficoms_Mw  },
  /* 3 */ { 0, &Ia_ficomps_Mw },
  /* 4 */ { 0, &Ia_fisubs_Mw  },
  /* 5 */ { 0, &Ia_fisubrs_Mw },
  /* 6 */ { 0, &Ia_fidivs_Mw  },
  /* 7 */ { 0, &Ia_fidivrs_Mw }
};

  // DF (modrm is outside 00h - BFh) (mod != 11)
static BxDisasmOpcodeTable_t BxDisasmFPGroupDF[8] = { 
  /* 0 */ { 0, &Ia_filds_Mw   },
  /* 1 */ { 0, &Ia_fisttps_Mw },
  /* 2 */ { 0, &Ia_fists_Mw   },
  /* 3 */ { 0, &Ia_fistps_Mw  },
  /* 4 */ { 0, &Ia_fbldt_Mt   },
  /* 5 */ { 0, &Ia_fildq_Mq   },
  /* 6 */ { 0, &Ia_fbstpt_Mt  },
  /* 7 */ { 0, &Ia_fistpq_Mq  }
};

// 512 entries for second byte of floating point instructions. (when mod==11b) 
static BxDisasmOpcodeTable_t BxDisasmOpcodeInfoFP[512] = { 
  // D8 (modrm is outside 00h - BFh) (mod == 11)
  /* D8 C0 */ { 0, &Ia_fadd_ST0_STi  },
  /* D8 C1 */ { 0, &Ia_fadd_ST0_STi  },
  /* D8 C2 */ { 0, &Ia_fadd_ST0_STi  },
  /* D8 C3 */ { 0, &Ia_fadd_ST0_STi  },
  /* D8 C4 */ { 0, &Ia_fadd_ST0_STi  },
  /* D8 C5 */ { 0, &Ia_fadd_ST0_STi  },
  /* D8 C6 */ { 0, &Ia_fadd_ST0_STi  },
  /* D8 C7 */ { 0, &Ia_fadd_ST0_STi  },
  /* D8 C8 */ { 0, &Ia_fmul_ST0_STi  },
  /* D8 C9 */ { 0, &Ia_fmul_ST0_STi  },
  /* D8 CA */ { 0, &Ia_fmul_ST0_STi  },
  /* D8 CB */ { 0, &Ia_fmul_ST0_STi  },
  /* D8 CC */ { 0, &Ia_fmul_ST0_STi  },
  /* D8 CD */ { 0, &Ia_fmul_ST0_STi  },
  /* D8 CE */ { 0, &Ia_fmul_ST0_STi  },
  /* D8 CF */ { 0, &Ia_fmul_ST0_STi  },
  /* D8 D0 */ { 0, &Ia_fcom_STi      },
  /* D8 D1 */ { 0, &Ia_fcom_STi      },
  /* D8 D2 */ { 0, &Ia_fcom_STi      },
  /* D8 D3 */ { 0, &Ia_fcom_STi      },
  /* D8 D4 */ { 0, &Ia_fcom_STi      },
  /* D8 D5 */ { 0, &Ia_fcom_STi      },
  /* D8 D6 */ { 0, &Ia_fcom_STi      },
  /* D8 D7 */ { 0, &Ia_fcom_STi      },
  /* D8 D8 */ { 0, &Ia_fcomp_STi     },
  /* D8 D9 */ { 0, &Ia_fcomp_STi     },
  /* D8 DA */ { 0, &Ia_fcomp_STi     },
  /* D8 DB */ { 0, &Ia_fcomp_STi     },
  /* D8 DC */ { 0, &Ia_fcomp_STi     },
  /* D8 DD */ { 0, &Ia_fcomp_STi     },
  /* D8 DE */ { 0, &Ia_fcomp_STi     },
  /* D8 DF */ { 0, &Ia_fcomp_STi     },
  /* D8 E0 */ { 0, &Ia_fsub_ST0_STi  },
  /* D8 E1 */ { 0, &Ia_fsub_ST0_STi  },
  /* D8 E2 */ { 0, &Ia_fsub_ST0_STi  },
  /* D8 E3 */ { 0, &Ia_fsub_ST0_STi  },
  /* D8 E4 */ { 0, &Ia_fsub_ST0_STi  },
  /* D8 E5 */ { 0, &Ia_fsub_ST0_STi  },
  /* D8 E6 */ { 0, &Ia_fsub_ST0_STi  },
  /* D8 E7 */ { 0, &Ia_fsub_ST0_STi  },
  /* D8 E8 */ { 0, &Ia_fsubr_ST0_STi },
  /* D8 E9 */ { 0, &Ia_fsubr_ST0_STi },
  /* D8 EA */ { 0, &Ia_fsubr_ST0_STi },
  /* D8 EB */ { 0, &Ia_fsubr_ST0_STi },
  /* D8 EC */ { 0, &Ia_fsubr_ST0_STi },
  /* D8 ED */ { 0, &Ia_fsubr_ST0_STi },
  /* D8 EE */ { 0, &Ia_fsubr_ST0_STi },
  /* D8 EF */ { 0, &Ia_fsubr_ST0_STi },
  /* D8 F0 */ { 0, &Ia_fdiv_ST0_STi  },
  /* D8 F1 */ { 0, &Ia_fdiv_ST0_STi  },
  /* D8 F2 */ { 0, &Ia_fdiv_ST0_STi  },
  /* D8 F3 */ { 0, &Ia_fdiv_ST0_STi  },
  /* D8 F4 */ { 0, &Ia_fdiv_ST0_STi  },
  /* D8 F5 */ { 0, &Ia_fdiv_ST0_STi  },
  /* D8 F6 */ { 0, &Ia_fdiv_ST0_STi  },
  /* D8 F7 */ { 0, &Ia_fdiv_ST0_STi  },
  /* D8 F8 */ { 0, &Ia_fdivr_ST0_STi },
  /* D8 F9 */ { 0, &Ia_fdivr_ST0_STi },
  /* D8 FA */ { 0, &Ia_fdivr_ST0_STi },
  /* D8 FB */ { 0, &Ia_fdivr_ST0_STi },
  /* D8 FC */ { 0, &Ia_fdivr_ST0_STi },
  /* D8 FD */ { 0, &Ia_fdivr_ST0_STi },
  /* D8 FE */ { 0, &Ia_fdivr_ST0_STi },
  /* D8 FF */ { 0, &Ia_fdivr_ST0_STi },
  
  // D9 (modrm is outside 00h - BFh) (mod == 11)
  /* D9 C0 */ { 0, &Ia_fld_STi },
  /* D9 C1 */ { 0, &Ia_fld_STi },
  /* D9 C2 */ { 0, &Ia_fld_STi },
  /* D9 C3 */ { 0, &Ia_fld_STi },
  /* D9 C4 */ { 0, &Ia_fld_STi },
  /* D9 C5 */ { 0, &Ia_fld_STi },
  /* D9 C6 */ { 0, &Ia_fld_STi },
  /* D9 C7 */ { 0, &Ia_fld_STi },
  /* D9 C8 */ { 0, &Ia_fxch    },
  /* D9 C9 */ { 0, &Ia_fxch    },
  /* D9 CA */ { 0, &Ia_fxch    },
  /* D9 CB */ { 0, &Ia_fxch    },
  /* D9 CC */ { 0, &Ia_fxch    },
  /* D9 CD */ { 0, &Ia_fxch    },
  /* D9 CE */ { 0, &Ia_fxch    },
  /* D9 CF */ { 0, &Ia_fxch    },
  /* D9 D0 */ { 0, &Ia_fnop    },
  /* D9 D1 */ { 0, &Ia_Invalid },
  /* D9 D2 */ { 0, &Ia_Invalid },
  /* D9 D3 */ { 0, &Ia_Invalid },
  /* D9 D4 */ { 0, &Ia_Invalid },
  /* D9 D5 */ { 0, &Ia_Invalid },
  /* D9 D6 */ { 0, &Ia_Invalid },
  /* D9 D7 */ { 0, &Ia_Invalid },
  /* D9 D8 */ { 0, &Ia_Invalid },
  /* D9 D9 */ { 0, &Ia_Invalid },
  /* D9 DA */ { 0, &Ia_Invalid },
  /* D9 DB */ { 0, &Ia_Invalid },
  /* D9 DC */ { 0, &Ia_Invalid },
  /* D9 DD */ { 0, &Ia_Invalid },
  /* D9 DE */ { 0, &Ia_Invalid },
  /* D9 DF */ { 0, &Ia_Invalid },
  /* D9 E0 */ { 0, &Ia_fchs    },
  /* D9 E1 */ { 0, &Ia_fabs    },
  /* D9 E2 */ { 0, &Ia_Invalid },
  /* D9 E3 */ { 0, &Ia_Invalid },
  /* D9 E4 */ { 0, &Ia_ftst    },
  /* D9 E5 */ { 0, &Ia_fxam    },
  /* D9 E6 */ { 0, &Ia_Invalid },
  /* D9 E7 */ { 0, &Ia_Invalid },
  /* D9 E8 */ { 0, &Ia_fld1    },
  /* D9 E9 */ { 0, &Ia_fldl2t  },
  /* D9 EA */ { 0, &Ia_fldl2e  },
  /* D9 EB */ { 0, &Ia_fldpi   },
  /* D9 EC */ { 0, &Ia_fldlg2  },
  /* D9 ED */ { 0, &Ia_fldln2  },
  /* D9 EE */ { 0, &Ia_fldz    },
  /* D9 EF */ { 0, &Ia_Invalid },
  /* D9 F0 */ { 0, &Ia_f2xm1   },
  /* D9 F1 */ { 0, &Ia_fyl2x   },
  /* D9 F2 */ { 0, &Ia_fptan   },
  /* D9 F3 */ { 0, &Ia_fpatan  },
  /* D9 F4 */ { 0, &Ia_fxtract },
  /* D9 F5 */ { 0, &Ia_fprem1  },
  /* D9 F6 */ { 0, &Ia_fdecstp },
  /* D9 F7 */ { 0, &Ia_fincstp },
  /* D9 F8 */ { 0, &Ia_fprem   },
  /* D9 F9 */ { 0, &Ia_fyl2xp1 },
  /* D9 FA */ { 0, &Ia_fsqrt   },
  /* D9 FB */ { 0, &Ia_fsincos },
  /* D9 FC */ { 0, &Ia_frndint },
  /* D9 FD */ { 0, &Ia_fscale  },
  /* D9 FE */ { 0, &Ia_fsin    },
  /* D9 FF */ { 0, &Ia_fcos    },
                                  
  // DA (modrm is outside 00h - BFh) (mod == 11)
  /* DA C0 */ { 0, &Ia_fcmovb_ST0_STi  },
  /* DA C1 */ { 0, &Ia_fcmovb_ST0_STi  },
  /* DA C2 */ { 0, &Ia_fcmovb_ST0_STi  },
  /* DA C3 */ { 0, &Ia_fcmovb_ST0_STi  },
  /* DA C4 */ { 0, &Ia_fcmovb_ST0_STi  },
  /* DA C5 */ { 0, &Ia_fcmovb_ST0_STi  },
  /* DA C6 */ { 0, &Ia_fcmovb_ST0_STi  },
  /* DA C7 */ { 0, &Ia_fcmovb_ST0_STi  },
  /* DA C8 */ { 0, &Ia_fcmove_ST0_STi  },
  /* DA C9 */ { 0, &Ia_fcmove_ST0_STi  },
  /* DA CA */ { 0, &Ia_fcmove_ST0_STi  },
  /* DA CB */ { 0, &Ia_fcmove_ST0_STi  },
  /* DA CC */ { 0, &Ia_fcmove_ST0_STi  },
  /* DA CD */ { 0, &Ia_fcmove_ST0_STi  },
  /* DA CE */ { 0, &Ia_fcmove_ST0_STi  },
  /* DA CF */ { 0, &Ia_fcmove_ST0_STi  },
  /* DA D0 */ { 0, &Ia_fcmovbe_ST0_STi },
  /* DA D1 */ { 0, &Ia_fcmovbe_ST0_STi },
  /* DA D2 */ { 0, &Ia_fcmovbe_ST0_STi },
  /* DA D3 */ { 0, &Ia_fcmovbe_ST0_STi },
  /* DA D4 */ { 0, &Ia_fcmovbe_ST0_STi },
  /* DA D5 */ { 0, &Ia_fcmovbe_ST0_STi },
  /* DA D6 */ { 0, &Ia_fcmovbe_ST0_STi },
  /* DA D7 */ { 0, &Ia_fcmovbe_ST0_STi },
  /* DA D8 */ { 0, &Ia_fcmovu_ST0_STi  },
  /* DA D9 */ { 0, &Ia_fcmovu_ST0_STi  },
  /* DA DA */ { 0, &Ia_fcmovu_ST0_STi  },
  /* DA DB */ { 0, &Ia_fcmovu_ST0_STi  },
  /* DA DC */ { 0, &Ia_fcmovu_ST0_STi  },
  /* DA DD */ { 0, &Ia_fcmovu_ST0_STi  },
  /* DA DE */ { 0, &Ia_fcmovu_ST0_STi  },
  /* DA DF */ { 0, &Ia_fcmovu_ST0_STi  },
  /* DA E0 */ { 0, &Ia_Invalid },
  /* DA E1 */ { 0, &Ia_Invalid },
  /* DA E2 */ { 0, &Ia_Invalid },
  /* DA E3 */ { 0, &Ia_Invalid },
  /* DA E4 */ { 0, &Ia_Invalid },
  /* DA E5 */ { 0, &Ia_Invalid },
  /* DA E6 */ { 0, &Ia_Invalid },
  /* DA E7 */ { 0, &Ia_Invalid },
  /* DA E8 */ { 0, &Ia_Invalid },
  /* DA E9 */ { 0, &Ia_fucompp },
  /* DA EA */ { 0, &Ia_Invalid },
  /* DA EB */ { 0, &Ia_Invalid },
  /* DA EC */ { 0, &Ia_Invalid },
  /* DA ED */ { 0, &Ia_Invalid },
  /* DA EE */ { 0, &Ia_Invalid },
  /* DA EF */ { 0, &Ia_Invalid },
  /* DA F0 */ { 0, &Ia_Invalid },
  /* DA F1 */ { 0, &Ia_Invalid },
  /* DA F2 */ { 0, &Ia_Invalid },
  /* DA F3 */ { 0, &Ia_Invalid },
  /* DA F4 */ { 0, &Ia_Invalid },
  /* DA F5 */ { 0, &Ia_Invalid },
  /* DA F6 */ { 0, &Ia_Invalid },
  /* DA F7 */ { 0, &Ia_Invalid },
  /* DA F8 */ { 0, &Ia_Invalid },
  /* DA F9 */ { 0, &Ia_Invalid },
  /* DA FA */ { 0, &Ia_Invalid },
  /* DA FB */ { 0, &Ia_Invalid },
  /* DA FC */ { 0, &Ia_Invalid },
  /* DA FD */ { 0, &Ia_Invalid },
  /* DA FE */ { 0, &Ia_Invalid },
  /* DA FF */ { 0, &Ia_Invalid },

  // DB (modrm is outside 00h - BFh) (mod == 11)
  /* DB C0 */ { 0, &Ia_fcmovnb_ST0_STi  },
  /* DB C1 */ { 0, &Ia_fcmovnb_ST0_STi  },
  /* DB C2 */ { 0, &Ia_fcmovnb_ST0_STi  },
  /* DB C3 */ { 0, &Ia_fcmovnb_ST0_STi  },
  /* DB C4 */ { 0, &Ia_fcmovnb_ST0_STi  },
  /* DB C5 */ { 0, &Ia_fcmovnb_ST0_STi  },
  /* DB C6 */ { 0, &Ia_fcmovnb_ST0_STi  },
  /* DB C7 */ { 0, &Ia_fcmovnb_ST0_STi  },
  /* DB C8 */ { 0, &Ia_fcmovne_ST0_STi  },
  /* DB C9 */ { 0, &Ia_fcmovne_ST0_STi  },
  /* DB CA */ { 0, &Ia_fcmovne_ST0_STi  },
  /* DB CB */ { 0, &Ia_fcmovne_ST0_STi  },
  /* DB CC */ { 0, &Ia_fcmovne_ST0_STi  },
  /* DB CD */ { 0, &Ia_fcmovne_ST0_STi  },
  /* DB CE */ { 0, &Ia_fcmovne_ST0_STi  },
  /* DB CF */ { 0, &Ia_fcmovne_ST0_STi  },
  /* DB D0 */ { 0, &Ia_fcmovnbe_ST0_STi },
  /* DB D1 */ { 0, &Ia_fcmovnbe_ST0_STi },
  /* DB D2 */ { 0, &Ia_fcmovnbe_ST0_STi },
  /* DB D3 */ { 0, &Ia_fcmovnbe_ST0_STi },
  /* DB D4 */ { 0, &Ia_fcmovnbe_ST0_STi },
  /* DB D5 */ { 0, &Ia_fcmovnbe_ST0_STi },
  /* DB D6 */ { 0, &Ia_fcmovnbe_ST0_STi },
  /* DB D7 */ { 0, &Ia_fcmovnbe_ST0_STi },
  /* DB D8 */ { 0, &Ia_fcmovnu_ST0_STi  },
  /* DB D9 */ { 0, &Ia_fcmovnu_ST0_STi  },
  /* DB DA */ { 0, &Ia_fcmovnu_ST0_STi  },
  /* DB DB */ { 0, &Ia_fcmovnu_ST0_STi  },
  /* DB DC */ { 0, &Ia_fcmovnu_ST0_STi  },
  /* DB DD */ { 0, &Ia_fcmovnu_ST0_STi  },
  /* DB DE */ { 0, &Ia_fcmovnu_ST0_STi  },
  /* DB DF */ { 0, &Ia_fcmovnu_ST0_STi  },
  /* DB E0 */ { 0, &Ia_feni    },
  /* DB E1 */ { 0, &Ia_fdisi   },
  /* DB E2 */ { 0, &Ia_fnclex  },
  /* DB E3 */ { 0, &Ia_fninit  },
  /* DB E4 */ { 0, &Ia_fsetpm  },
  /* DB E5 */ { 0, &Ia_Invalid },
  /* DB E6 */ { 0, &Ia_Invalid },
  /* DB E7 */ { 0, &Ia_Invalid },
  /* DB E8 */ { 0, &Ia_fucomi_ST0_STi },
  /* DB E9 */ { 0, &Ia_fucomi_ST0_STi },
  /* DB EA */ { 0, &Ia_fucomi_ST0_STi },
  /* DB EB */ { 0, &Ia_fucomi_ST0_STi },
  /* DB EC */ { 0, &Ia_fucomi_ST0_STi },
  /* DB ED */ { 0, &Ia_fucomi_ST0_STi },
  /* DB EE */ { 0, &Ia_fucomi_ST0_STi },
  /* DB EF */ { 0, &Ia_fucomi_ST0_STi },
  /* DB F0 */ { 0, &Ia_fcomi_ST0_STi  },
  /* DB F1 */ { 0, &Ia_fcomi_ST0_STi  },
  /* DB F2 */ { 0, &Ia_fcomi_ST0_STi  },
  /* DB F3 */ { 0, &Ia_fcomi_ST0_STi  },
  /* DB F4 */ { 0, &Ia_fcomi_ST0_STi  },
  /* DB F5 */ { 0, &Ia_fcomi_ST0_STi  },
  /* DB F6 */ { 0, &Ia_fcomi_ST0_STi  },
  /* DB F7 */ { 0, &Ia_fcomi_ST0_STi  },
  /* DB F8 */ { 0, &Ia_Invalid },
  /* DB F9 */ { 0, &Ia_Invalid },
  /* DB FA */ { 0, &Ia_Invalid },
  /* DB FB */ { 0, &Ia_Invalid },
  /* DB FC */ { 0, &Ia_Invalid },
  /* DB FD */ { 0, &Ia_Invalid },
  /* DB FE */ { 0, &Ia_Invalid },
  /* DB FF */ { 0, &Ia_Invalid },

  // DC (modrm is outside 00h - BFh) (mod == 11)
  /* DC C0 */ { 0, &Ia_fadd_STi_ST0 },
  /* DC C1 */ { 0, &Ia_fadd_STi_ST0 },
  /* DC C2 */ { 0, &Ia_fadd_STi_ST0 },
  /* DC C3 */ { 0, &Ia_fadd_STi_ST0 },
  /* DC C4 */ { 0, &Ia_fadd_STi_ST0 },
  /* DC C5 */ { 0, &Ia_fadd_STi_ST0 },
  /* DC C6 */ { 0, &Ia_fadd_STi_ST0 },
  /* DC C7 */ { 0, &Ia_fadd_STi_ST0 },
  /* DC C8 */ { 0, &Ia_fmul_STi_ST0 },
  /* DC C9 */ { 0, &Ia_fmul_STi_ST0 },
  /* DC CA */ { 0, &Ia_fmul_STi_ST0 },
  /* DC CB */ { 0, &Ia_fmul_STi_ST0 },
  /* DC CC */ { 0, &Ia_fmul_STi_ST0 },
  /* DC CD */ { 0, &Ia_fmul_STi_ST0 },
  /* DC CE */ { 0, &Ia_fmul_STi_ST0 },
  /* DC CF */ { 0, &Ia_fmul_STi_ST0 },
  /* DC D0 */ { 0, &Ia_Invalid },
  /* DC D1 */ { 0, &Ia_Invalid },
  /* DC D2 */ { 0, &Ia_Invalid },
  /* DC D3 */ { 0, &Ia_Invalid },
  /* DC D4 */ { 0, &Ia_Invalid },
  /* DC D5 */ { 0, &Ia_Invalid },
  /* DC D6 */ { 0, &Ia_Invalid },
  /* DC D7 */ { 0, &Ia_Invalid },
  /* DC D8 */ { 0, &Ia_Invalid },
  /* DC D9 */ { 0, &Ia_Invalid },
  /* DC DA */ { 0, &Ia_Invalid },
  /* DC DB */ { 0, &Ia_Invalid },
  /* DC DC */ { 0, &Ia_Invalid },
  /* DC DD */ { 0, &Ia_Invalid },
  /* DC DE */ { 0, &Ia_Invalid },
  /* DC DF */ { 0, &Ia_Invalid },
  /* DC E0 */ { 0, &Ia_fsubr_STi_ST0 },
  /* DC E1 */ { 0, &Ia_fsubr_STi_ST0 },
  /* DC E2 */ { 0, &Ia_fsubr_STi_ST0 },
  /* DC E3 */ { 0, &Ia_fsubr_STi_ST0 },
  /* DC E4 */ { 0, &Ia_fsubr_STi_ST0 },
  /* DC E5 */ { 0, &Ia_fsubr_STi_ST0 },
  /* DC E6 */ { 0, &Ia_fsubr_STi_ST0 },
  /* DC E7 */ { 0, &Ia_fsubr_STi_ST0 },
  /* DC E8 */ { 0, &Ia_fsub_STi_ST0  },
  /* DC E9 */ { 0, &Ia_fsub_STi_ST0  },
  /* DC EA */ { 0, &Ia_fsub_STi_ST0  },
  /* DC EB */ { 0, &Ia_fsub_STi_ST0  },
  /* DC EC */ { 0, &Ia_fsub_STi_ST0  },
  /* DC ED */ { 0, &Ia_fsub_STi_ST0  },
  /* DC EE */ { 0, &Ia_fsub_STi_ST0  },
  /* DC EF */ { 0, &Ia_fsub_STi_ST0  },
  /* DC F0 */ { 0, &Ia_fdivr_STi_ST0 },
  /* DC F1 */ { 0, &Ia_fdivr_STi_ST0 },
  /* DC F2 */ { 0, &Ia_fdivr_STi_ST0 },
  /* DC F3 */ { 0, &Ia_fdivr_STi_ST0 },
  /* DC F4 */ { 0, &Ia_fdivr_STi_ST0 },
  /* DC F5 */ { 0, &Ia_fdivr_STi_ST0 },
  /* DC F6 */ { 0, &Ia_fdivr_STi_ST0 },
  /* DC F7 */ { 0, &Ia_fdivr_STi_ST0 },
  /* DC F8 */ { 0, &Ia_fdiv_STi_ST0  },
  /* DC F9 */ { 0, &Ia_fdiv_STi_ST0  },
  /* DC FA */ { 0, &Ia_fdiv_STi_ST0  },
  /* DC FB */ { 0, &Ia_fdiv_STi_ST0  },
  /* DC FC */ { 0, &Ia_fdiv_STi_ST0  },
  /* DC FD */ { 0, &Ia_fdiv_STi_ST0  },
  /* DC FE */ { 0, &Ia_fdiv_STi_ST0  },
  /* DC FF */ { 0, &Ia_fdiv_STi_ST0  },

  // DD (modrm is outside 00h - BFh) (mod == 11)
  /* DD C0 */ { 0, &Ia_ffree_STi  },
  /* DD C1 */ { 0, &Ia_ffree_STi  },
  /* DD C2 */ { 0, &Ia_ffree_STi  },
  /* DD C3 */ { 0, &Ia_ffree_STi  },
  /* DD C4 */ { 0, &Ia_ffree_STi  },
  /* DD C5 */ { 0, &Ia_ffree_STi  },
  /* DD C6 */ { 0, &Ia_ffree_STi  },
  /* DD C7 */ { 0, &Ia_ffree_STi  },
  /* DD C8 */ { 0, &Ia_Invalid    },
  /* DD C9 */ { 0, &Ia_Invalid    },
  /* DD CA */ { 0, &Ia_Invalid    },
  /* DD CB */ { 0, &Ia_Invalid    },
  /* DD CC */ { 0, &Ia_Invalid    },
  /* DD CD */ { 0, &Ia_Invalid    },
  /* DD CE */ { 0, &Ia_Invalid    },
  /* DD CF */ { 0, &Ia_Invalid    },
  /* DD D0 */ { 0, &Ia_fst_STi    },
  /* DD D1 */ { 0, &Ia_fst_STi    },
  /* DD D2 */ { 0, &Ia_fst_STi    },
  /* DD D3 */ { 0, &Ia_fst_STi    },
  /* DD D4 */ { 0, &Ia_fst_STi    },
  /* DD D5 */ { 0, &Ia_fst_STi    },
  /* DD D6 */ { 0, &Ia_fst_STi    },
  /* DD D7 */ { 0, &Ia_fst_STi    },
  /* DD D8 */ { 0, &Ia_fstp_STi   },
  /* DD D9 */ { 0, &Ia_fstp_STi   },
  /* DD DA */ { 0, &Ia_fstp_STi   },
  /* DD DB */ { 0, &Ia_fstp_STi   },
  /* DD DC */ { 0, &Ia_fstp_STi   },
  /* DD DD */ { 0, &Ia_fstp_STi   },
  /* DD DE */ { 0, &Ia_fstp_STi   },
  /* DD DF */ { 0, &Ia_fstp_STi   },
  /* DD E0 */ { 0, &Ia_fucom_STi  },
  /* DD E1 */ { 0, &Ia_fucom_STi  },
  /* DD E2 */ { 0, &Ia_fucom_STi  },
  /* DD E3 */ { 0, &Ia_fucom_STi  },
  /* DD E4 */ { 0, &Ia_fucom_STi  },
  /* DD E5 */ { 0, &Ia_fucom_STi  },
  /* DD E6 */ { 0, &Ia_fucom_STi  },
  /* DD E7 */ { 0, &Ia_fucom_STi  },
  /* DD E8 */ { 0, &Ia_fucomp_STi },
  /* DD E9 */ { 0, &Ia_fucomp_STi },
  /* DD EA */ { 0, &Ia_fucomp_STi },
  /* DD EB */ { 0, &Ia_fucomp_STi },
  /* DD EC */ { 0, &Ia_fucomp_STi },
  /* DD ED */ { 0, &Ia_fucomp_STi },
  /* DD EE */ { 0, &Ia_fucomp_STi },
  /* DD EF */ { 0, &Ia_fucomp_STi },
  /* DD F0 */ { 0, &Ia_Invalid },
  /* DD F1 */ { 0, &Ia_Invalid },
  /* DD F2 */ { 0, &Ia_Invalid },
  /* DD F3 */ { 0, &Ia_Invalid },
  /* DD F4 */ { 0, &Ia_Invalid },
  /* DD F5 */ { 0, &Ia_Invalid },
  /* DD F6 */ { 0, &Ia_Invalid },
  /* DD F7 */ { 0, &Ia_Invalid },
  /* DD F8 */ { 0, &Ia_Invalid },
  /* DD F9 */ { 0, &Ia_Invalid },
  /* DD FA */ { 0, &Ia_Invalid },
  /* DD FB */ { 0, &Ia_Invalid },
  /* DD FC */ { 0, &Ia_Invalid },
  /* DD FD */ { 0, &Ia_Invalid },
  /* DD FE */ { 0, &Ia_Invalid },
  /* DD FF */ { 0, &Ia_Invalid },

  // DE (modrm is outside 00h - BFh) (mod == 11)
  /* DE C0 */ { 0, &Ia_faddp_STi_ST0 },
  /* DE C1 */ { 0, &Ia_faddp_STi_ST0 },
  /* DE C2 */ { 0, &Ia_faddp_STi_ST0 },
  /* DE C3 */ { 0, &Ia_faddp_STi_ST0 },
  /* DE C4 */ { 0, &Ia_faddp_STi_ST0 },
  /* DE C5 */ { 0, &Ia_faddp_STi_ST0 },
  /* DE C6 */ { 0, &Ia_faddp_STi_ST0 },
  /* DE C7 */ { 0, &Ia_faddp_STi_ST0 },
  /* DE C8 */ { 0, &Ia_fmulp_STi_ST0 },
  /* DE C9 */ { 0, &Ia_fmulp_STi_ST0 },
  /* DE CA */ { 0, &Ia_fmulp_STi_ST0 },
  /* DE CB */ { 0, &Ia_fmulp_STi_ST0 },
  /* DE CC */ { 0, &Ia_fmulp_STi_ST0 },
  /* DE CD */ { 0, &Ia_fmulp_STi_ST0 },
  /* DE CE */ { 0, &Ia_fmulp_STi_ST0 },
  /* DE CF */ { 0, &Ia_fmulp_STi_ST0 },
  /* DE D0 */ { 0, &Ia_Invalid },
  /* DE D1 */ { 0, &Ia_Invalid },
  /* DE D2 */ { 0, &Ia_Invalid },
  /* DE D3 */ { 0, &Ia_Invalid },
  /* DE D4 */ { 0, &Ia_Invalid },
  /* DE D5 */ { 0, &Ia_Invalid },
  /* DE D6 */ { 0, &Ia_Invalid },
  /* DE D7 */ { 0, &Ia_Invalid },
  /* DE D8 */ { 0, &Ia_Invalid },
  /* DE D9 */ { 0, &Ia_fcompp  },
  /* DE DA */ { 0, &Ia_Invalid },
  /* DE DB */ { 0, &Ia_Invalid },
  /* DE DC */ { 0, &Ia_Invalid },
  /* DE DD */ { 0, &Ia_Invalid },
  /* DE DE */ { 0, &Ia_Invalid },
  /* DE DF */ { 0, &Ia_Invalid },
  /* DE E0 */ { 0, &Ia_fsubrp_STi_ST0 },
  /* DE E1 */ { 0, &Ia_fsubrp_STi_ST0 },
  /* DE E2 */ { 0, &Ia_fsubrp_STi_ST0 },
  /* DE E3 */ { 0, &Ia_fsubrp_STi_ST0 },
  /* DE E4 */ { 0, &Ia_fsubrp_STi_ST0 },
  /* DE E5 */ { 0, &Ia_fsubrp_STi_ST0 },
  /* DE E6 */ { 0, &Ia_fsubrp_STi_ST0 },
  /* DE E7 */ { 0, &Ia_fsubrp_STi_ST0 },
  /* DE E8 */ { 0, &Ia_fsubp_STi_ST0  },
  /* DE E9 */ { 0, &Ia_fsubp_STi_ST0  },
  /* DE EA */ { 0, &Ia_fsubp_STi_ST0  },
  /* DE EB */ { 0, &Ia_fsubp_STi_ST0  },
  /* DE EC */ { 0, &Ia_fsubp_STi_ST0  },
  /* DE ED */ { 0, &Ia_fsubp_STi_ST0  },
  /* DE EE */ { 0, &Ia_fsubp_STi_ST0  },
  /* DE EF */ { 0, &Ia_fsubp_STi_ST0  },
  /* DE F0 */ { 0, &Ia_fdivrp_STi_ST0 },
  /* DE F1 */ { 0, &Ia_fdivrp_STi_ST0 },
  /* DE F2 */ { 0, &Ia_fdivrp_STi_ST0 },
  /* DE F3 */ { 0, &Ia_fdivrp_STi_ST0 },
  /* DE F4 */ { 0, &Ia_fdivrp_STi_ST0 },
  /* DE F5 */ { 0, &Ia_fdivrp_STi_ST0 },
  /* DE F6 */ { 0, &Ia_fdivrp_STi_ST0 },
  /* DE F7 */ { 0, &Ia_fdivrp_STi_ST0 },
  /* DE F8 */ { 0, &Ia_fdivp_STi_ST0  },
  /* DE F9 */ { 0, &Ia_fdivp_STi_ST0  },
  /* DE FA */ { 0, &Ia_fdivp_STi_ST0  },
  /* DE FB */ { 0, &Ia_fdivp_STi_ST0  },
  /* DE FC */ { 0, &Ia_fdivp_STi_ST0  },
  /* DE FD */ { 0, &Ia_fdivp_STi_ST0  },
  /* DE FE */ { 0, &Ia_fdivp_STi_ST0  },
  /* DE FF */ { 0, &Ia_fdivp_STi_ST0  },

  // DF (modrm is outside 00h - BFh) (mod == 11)
  /* DF C0 */ { 0, &Ia_ffreep_STi }, // 287 compatibility opcode
  /* DF C1 */ { 0, &Ia_ffreep_STi },
  /* DF C2 */ { 0, &Ia_ffreep_STi },
  /* DF C3 */ { 0, &Ia_ffreep_STi },
  /* DF C4 */ { 0, &Ia_ffreep_STi },
  /* DF C5 */ { 0, &Ia_ffreep_STi },
  /* DF C6 */ { 0, &Ia_ffreep_STi },
  /* DF C7 */ { 0, &Ia_ffreep_STi },
  /* DF C8 */ { 0, &Ia_Invalid },
  /* DF C9 */ { 0, &Ia_Invalid },
  /* DF CA */ { 0, &Ia_Invalid },
  /* DF CB */ { 0, &Ia_Invalid },
  /* DF CC */ { 0, &Ia_Invalid },
  /* DF CD */ { 0, &Ia_Invalid },
  /* DF CE */ { 0, &Ia_Invalid },
  /* DF CF */ { 0, &Ia_Invalid },
  /* DF D0 */ { 0, &Ia_Invalid },
  /* DF D1 */ { 0, &Ia_Invalid },
  /* DF D2 */ { 0, &Ia_Invalid },
  /* DF D3 */ { 0, &Ia_Invalid },
  /* DF D4 */ { 0, &Ia_Invalid },
  /* DF D5 */ { 0, &Ia_Invalid },
  /* DF D6 */ { 0, &Ia_Invalid },
  /* DF D7 */ { 0, &Ia_Invalid },
  /* DF D8 */ { 0, &Ia_Invalid },
  /* DF D9 */ { 0, &Ia_Invalid },
  /* DF DA */ { 0, &Ia_Invalid },
  /* DF DB */ { 0, &Ia_Invalid },
  /* DF DC */ { 0, &Ia_Invalid },
  /* DF DD */ { 0, &Ia_Invalid },
  /* DF DE */ { 0, &Ia_Invalid },
  /* DF DF */ { 0, &Ia_Invalid },
  /* DF E0 */ { 0, &Ia_fnstsw_AX },
  /* DF E1 */ { 0, &Ia_Invalid },
  /* DF E2 */ { 0, &Ia_Invalid },
  /* DF E3 */ { 0, &Ia_Invalid },
  /* DF E4 */ { 0, &Ia_Invalid },
  /* DF E5 */ { 0, &Ia_Invalid },
  /* DF E6 */ { 0, &Ia_Invalid },
  /* DF E7 */ { 0, &Ia_Invalid },
  /* DF E8 */ { 0, &Ia_fucomip_ST0_STi },
  /* DF E9 */ { 0, &Ia_fucomip_ST0_STi },
  /* DF EA */ { 0, &Ia_fucomip_ST0_STi },
  /* DF EB */ { 0, &Ia_fucomip_ST0_STi },
  /* DF EC */ { 0, &Ia_fucomip_ST0_STi },
  /* DF ED */ { 0, &Ia_fucomip_ST0_STi },
  /* DF EE */ { 0, &Ia_fucomip_ST0_STi },
  /* DF EF */ { 0, &Ia_fucomip_ST0_STi },
  /* DF F0 */ { 0, &Ia_fcomip_ST0_STi  },
  /* DF F1 */ { 0, &Ia_fcomip_ST0_STi  },
  /* DF F2 */ { 0, &Ia_fcomip_ST0_STi  },
  /* DF F3 */ { 0, &Ia_fcomip_ST0_STi  },
  /* DF F4 */ { 0, &Ia_fcomip_ST0_STi  },
  /* DF F5 */ { 0, &Ia_fcomip_ST0_STi  },
  /* DF F6 */ { 0, &Ia_fcomip_ST0_STi  },
  /* DF F7 */ { 0, &Ia_fcomip_ST0_STi  },
  /* DF F8 */ { 0, &Ia_Invalid },
  /* DF F9 */ { 0, &Ia_Invalid },
  /* DF FA */ { 0, &Ia_Invalid },
  /* DF FB */ { 0, &Ia_Invalid },
  /* DF FC */ { 0, &Ia_Invalid },
  /* DF FD */ { 0, &Ia_Invalid },
  /* DF FE */ { 0, &Ia_Invalid },
  /* DF FF */ { 0, &Ia_Invalid },
};

/* ************************************************************************ */
/* 3-byte opcode table (Table A-4, 0F 38) */

static BxDisasmOpcodeTable_t BxDisasm3ByteOp0f380x[16] = {
  /* 00 */ { GRPSSE(0f3800) },
  /* 01 */ { GRPSSE(0f3801) },
  /* 02 */ { GRPSSE(0f3802) },
  /* 03 */ { GRPSSE(0f3803) },
  /* 04 */ { GRPSSE(0f3804) },
  /* 05 */ { GRPSSE(0f3805) },
  /* 06 */ { GRPSSE(0f3806) },
  /* 07 */ { GRPSSE(0f3807) },
  /* 08 */ { GRPSSE(0f3808) },
  /* 09 */ { GRPSSE(0f3809) },
  /* 0A */ { GRPSSE(0f380a) },
  /* 0B */ { GRPSSE(0f380b) },
  /* 0C */ { 0, &Ia_Invalid },
  /* 0D */ { 0, &Ia_Invalid },
  /* 0E */ { 0, &Ia_Invalid },
  /* 0F */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasm3ByteOp0f381x[16] = {
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
  /* 0C */ { GRPSSE(0f381c) },
  /* 0D */ { GRPSSE(0f381d) },
  /* 0E */ { GRPSSE(0f381e) },
  /* 0F */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasm3ByteTableA4[16] = {
  /* 00 */ { GR3BOP(0f380x) },
  /* 01 */ { GR3BOP(0f381x) },
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
  /* 0F */ { 0, &Ia_Invalid }
};

/* ************************************************************************ */
/* 3-byte opcode table (Table A-5, 0F 3A) */

static BxDisasmOpcodeTable_t BxDisasm3ByteOp0f3a0x[16] = {
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
  /* 0F */ { GRPSSE(0f3a0f) }
};

static BxDisasmOpcodeTable_t BxDisasm3ByteTableA5[16] = {
  /* 00 */ { GR3BOP(0f3a0x) },
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
  /* 0F */ { 0, &Ia_Invalid }
};

/* ************************************************************************ */
/* 16-bit operand size */

static BxDisasmOpcodeTable_t BxDisasmOpcodes16[256*2] = {
  // 256 entries for single byte opcodes
  /* 00 */ { 0, &Ia_addb_Eb_Gb },
  /* 01 */ { 0, &Ia_addw_Ew_Gw },
  /* 02 */ { 0, &Ia_addb_Gb_Eb },
  /* 03 */ { 0, &Ia_addw_Gw_Ew },
  /* 04 */ { 0, &Ia_addb_AL_Ib },
  /* 05 */ { 0, &Ia_addw_AX_Iw },
  /* 06 */ { 0, &Ia_pushw_ES   },
  /* 07 */ { 0, &Ia_popw_ES    },
  /* 08 */ { 0, &Ia_orb_Eb_Gb  },
  /* 09 */ { 0, &Ia_orw_Ew_Gw  },
  /* 0A */ { 0, &Ia_orb_Gb_Eb  },
  /* 0B */ { 0, &Ia_orw_Gw_Ew  },
  /* 0C */ { 0, &Ia_orb_AL_Ib  },
  /* 0D */ { 0, &Ia_orw_AX_Iw  },
  /* 0E */ { 0, &Ia_pushw_CS   },
  /* 0F */ { 0, &Ia_error      },   // 2 byte escape
  /* 10 */ { 0, &Ia_adcb_Eb_Gb },
  /* 11 */ { 0, &Ia_adcw_Ew_Gw },
  /* 12 */ { 0, &Ia_adcb_Gb_Eb },
  /* 13 */ { 0, &Ia_adcw_Gw_Ew },
  /* 14 */ { 0, &Ia_adcb_AL_Ib },
  /* 15 */ { 0, &Ia_adcw_AX_Iw },
  /* 16 */ { 0, &Ia_pushw_SS   },
  /* 17 */ { 0, &Ia_popw_SS    },
  /* 18 */ { 0, &Ia_sbbb_Eb_Gb },
  /* 19 */ { 0, &Ia_sbbw_Ew_Gw },
  /* 1A */ { 0, &Ia_sbbb_Gb_Eb },
  /* 1B */ { 0, &Ia_sbbw_Gw_Ew },
  /* 1C */ { 0, &Ia_sbbb_AL_Ib },
  /* 1D */ { 0, &Ia_sbbw_AX_Iw },
  /* 1E */ { 0, &Ia_pushw_DS   },
  /* 1F */ { 0, &Ia_popw_DS    },
  /* 20 */ { 0, &Ia_andb_Eb_Gb },
  /* 21 */ { 0, &Ia_andw_Ew_Gw },
  /* 22 */ { 0, &Ia_andb_Gb_Eb },
  /* 23 */ { 0, &Ia_andw_Gw_Ew },
  /* 24 */ { 0, &Ia_andb_AL_Ib },
  /* 25 */ { 0, &Ia_andw_AX_Iw },
  /* 26 */ { 0, &Ia_prefix_es  },   // ES:
  /* 27 */ { 0, &Ia_daa        },
  /* 28 */ { 0, &Ia_subb_Eb_Gb },
  /* 29 */ { 0, &Ia_subw_Ew_Gw },
  /* 2A */ { 0, &Ia_subb_Gb_Eb },
  /* 2B */ { 0, &Ia_subw_Gw_Ew },
  /* 2C */ { 0, &Ia_subb_AL_Ib },
  /* 2D */ { 0, &Ia_subw_AX_Iw },
  /* 2E */ { 0, &Ia_prefix_cs  },   // CS:
  /* 2F */ { 0, &Ia_das        },
  /* 30 */ { 0, &Ia_xorb_Eb_Gb },
  /* 31 */ { 0, &Ia_xorw_Ew_Gw },
  /* 32 */ { 0, &Ia_xorb_Gb_Eb },
  /* 33 */ { 0, &Ia_xorw_Gw_Ew },
  /* 34 */ { 0, &Ia_xorb_AL_Ib },
  /* 35 */ { 0, &Ia_xorw_AX_Iw },
  /* 36 */ { 0, &Ia_prefix_ss  },   // SS:
  /* 37 */ { 0, &Ia_aaa        },
  /* 38 */ { 0, &Ia_cmpb_Eb_Gb },
  /* 39 */ { 0, &Ia_cmpw_Ew_Gw },
  /* 3A */ { 0, &Ia_cmpb_Gb_Eb },
  /* 3B */ { 0, &Ia_cmpw_Gw_Ew },
  /* 3C */ { 0, &Ia_cmpb_AL_Ib },
  /* 3D */ { 0, &Ia_cmpw_AX_Iw },
  /* 3E */ { 0, &Ia_prefix_ds  },   // DS:
  /* 3F */ { 0, &Ia_aas        },
  /* 40 */ { 0, &Ia_incw_RX    },
  /* 41 */ { 0, &Ia_incw_RX    },
  /* 42 */ { 0, &Ia_incw_RX    },
  /* 43 */ { 0, &Ia_incw_RX    },
  /* 44 */ { 0, &Ia_incw_RX    },
  /* 45 */ { 0, &Ia_incw_RX    },
  /* 46 */ { 0, &Ia_incw_RX    },
  /* 47 */ { 0, &Ia_incw_RX    },
  /* 48 */ { 0, &Ia_decw_RX    },
  /* 49 */ { 0, &Ia_decw_RX    },
  /* 4A */ { 0, &Ia_decw_RX    },
  /* 4B */ { 0, &Ia_decw_RX    },
  /* 4C */ { 0, &Ia_decw_RX    },
  /* 4D */ { 0, &Ia_decw_RX    },
  /* 4E */ { 0, &Ia_decw_RX    },
  /* 4F */ { 0, &Ia_decw_RX    },
  /* 50 */ { 0, &Ia_pushw_RX   },
  /* 51 */ { 0, &Ia_pushw_RX   },
  /* 52 */ { 0, &Ia_pushw_RX   },
  /* 53 */ { 0, &Ia_pushw_RX   },
  /* 54 */ { 0, &Ia_pushw_RX   },
  /* 55 */ { 0, &Ia_pushw_RX   },
  /* 56 */ { 0, &Ia_pushw_RX   },
  /* 57 */ { 0, &Ia_pushw_RX   },
  /* 58 */ { 0, &Ia_popw_RX    },
  /* 59 */ { 0, &Ia_popw_RX    },
  /* 5A */ { 0, &Ia_popw_RX    },
  /* 5B */ { 0, &Ia_popw_RX    },
  /* 5C */ { 0, &Ia_popw_RX    },
  /* 5D */ { 0, &Ia_popw_RX    },
  /* 5E */ { 0, &Ia_popw_RX    },
  /* 5F */ { 0, &Ia_popw_RX    },
  /* 60 */ { 0, &Ia_pushaw     },
  /* 61 */ { 0, &Ia_popaw      },
  /* 62 */ { 0, &Ia_boundw_Gw_Ma },
  /* 63 */ { 0, &Ia_arpl_Ew_Rw },
  /* 64 */ { 0, &Ia_prefix_fs  },   // FS:
  /* 65 */ { 0, &Ia_prefix_gs  },   // GS:
  /* 66 */ { 0, &Ia_prefix_osize }, // OSIZE:
  /* 67 */ { 0, &Ia_prefix_asize }, // ASIZE:
  /* 68 */ { 0, &Ia_pushw_Iw   },
  /* 69 */ { 0, &Ia_imulw_Gw_Ew_Iw  },
  /* 6A */ { 0, &Ia_pushw_sIb   },
  /* 6B */ { 0, &Ia_imulw_Gw_Ew_sIb },
  /* 6C */ { 0, &Ia_insb_Yb_DX  },
  /* 6D */ { 0, &Ia_insw_Yw_DX  },
  /* 6E */ { 0, &Ia_outsb_DX_Xb },
  /* 6F */ { 0, &Ia_outsw_DX_Xw },
  /* 70 */ { 0, &Ia_jo_Jb       },
  /* 71 */ { 0, &Ia_jno_Jb      },
  /* 72 */ { 0, &Ia_jb_Jb       },
  /* 73 */ { 0, &Ia_jnb_Jb      },
  /* 74 */ { 0, &Ia_jz_Jb       },
  /* 75 */ { 0, &Ia_jnz_Jb      },
  /* 76 */ { 0, &Ia_jbe_Jb      },
  /* 77 */ { 0, &Ia_jnbe_Jb     },
  /* 78 */ { 0, &Ia_js_Jb       },
  /* 79 */ { 0, &Ia_jns_Jb      },
  /* 7A */ { 0, &Ia_jp_Jb       },
  /* 7B */ { 0, &Ia_jnp_Jb      },
  /* 7C */ { 0, &Ia_jl_Jb       },
  /* 7D */ { 0, &Ia_jnl_Jb      },
  /* 7E */ { 0, &Ia_jle_Jb      },
  /* 7F */ { 0, &Ia_jnle_Jb     },
  /* 80 */ { GRPN(G1EbIb)       },
  /* 81 */ { GRPN(G1EwIw)       },
  /* 82 */ { GRPN(G1EbIb)       },
  /* 83 */ { GRPN(G1EwIb)       },
  /* 84 */ { 0, &Ia_testb_Eb_Gb },
  /* 85 */ { 0, &Ia_testw_Ew_Gw },
  /* 86 */ { 0, &Ia_xchgb_Eb_Gb },
  /* 87 */ { 0, &Ia_xchgw_Ew_Gw },
  /* 88 */ { 0, &Ia_movb_Eb_Gb  },
  /* 89 */ { 0, &Ia_movw_Ew_Gw  },
  /* 8A */ { 0, &Ia_movb_Gb_Eb  },
  /* 8B */ { 0, &Ia_movw_Gw_Ew  },
  /* 8C */ { 0, &Ia_movw_Ew_Sw  },
  /* 8D */ { 0, &Ia_leaw_Gw_Mw  },
  /* 8E */ { 0, &Ia_movw_Sw_Ew  },
  /* 8F */ { 0, &Ia_popw_Ew     },
  /* 90 */ { 0, &Ia_nop         },
  /* 91 */ { 0, &Ia_xchgw_RX_AX },
  /* 92 */ { 0, &Ia_xchgw_RX_AX },
  /* 93 */ { 0, &Ia_xchgw_RX_AX },
  /* 94 */ { 0, &Ia_xchgw_RX_AX },
  /* 95 */ { 0, &Ia_xchgw_RX_AX },
  /* 96 */ { 0, &Ia_xchgw_RX_AX },
  /* 97 */ { 0, &Ia_xchgw_RX_AX },
  /* 98 */ { 0, &Ia_cbw         },
  /* 99 */ { 0, &Ia_cwd         },
  /* 9A */ { 0, &Ia_lcall_Apw   },
  /* 9B */ { 0, &Ia_fwait       },
  /* 9C */ { 0, &Ia_pushfw      },
  /* 9D */ { 0, &Ia_popfw       },
  /* 9E */ { 0, &Ia_sahf        },
  /* 9F */ { 0, &Ia_lahf        },
  /* A0 */ { 0, &Ia_movb_AL_Ob  },
  /* A1 */ { 0, &Ia_movw_AX_Ow  },
  /* A0 */ { 0, &Ia_movb_Ob_AL  },
  /* A1 */ { 0, &Ia_movw_Ow_AX  },
  /* A4 */ { 0, &Ia_movsb_Yb_Xb },
  /* A5 */ { 0, &Ia_movsw_Yw_Xw },
  /* A6 */ { 0, &Ia_cmpsb_Yb_Xb },
  /* A7 */ { 0, &Ia_cmpsw_Yw_Xw },
  /* A8 */ { 0, &Ia_testb_AL_Ib },
  /* A9 */ { 0, &Ia_testw_AX_Iw },
  /* AA */ { 0, &Ia_stosb_Yb_AL },
  /* AB */ { 0, &Ia_stosw_Yw_AX },
  /* AC */ { 0, &Ia_lodsb_AL_Xb },
  /* AD */ { 0, &Ia_lodsw_AX_Xw },
  /* AE */ { 0, &Ia_scasb_Yb_AL },
  /* AF */ { 0, &Ia_scasw_Yw_AX },
  /* B0 */ { 0, &Ia_movb_R8_Ib  },
  /* B1 */ { 0, &Ia_movb_R8_Ib  },
  /* B2 */ { 0, &Ia_movb_R8_Ib  },
  /* B3 */ { 0, &Ia_movb_R8_Ib  },
  /* B4 */ { 0, &Ia_movb_R8_Ib  },
  /* B5 */ { 0, &Ia_movb_R8_Ib  },
  /* B6 */ { 0, &Ia_movb_R8_Ib  },
  /* B7 */ { 0, &Ia_movb_R8_Ib  },
  /* B8 */ { 0, &Ia_movw_RX_Iw  },
  /* B9 */ { 0, &Ia_movw_RX_Iw  },
  /* BA */ { 0, &Ia_movw_RX_Iw  },
  /* BB */ { 0, &Ia_movw_RX_Iw  },
  /* BC */ { 0, &Ia_movw_RX_Iw  },
  /* BD */ { 0, &Ia_movw_RX_Iw  },
  /* BE */ { 0, &Ia_movw_RX_Iw  },
  /* BF */ { 0, &Ia_movw_RX_Iw  },
  /* C0 */ { GRPN(G2Eb)         },
  /* C1 */ { GRPN(G2Ew)         },
  /* C2 */ { 0, &Ia_ret_Iw      },
  /* C3 */ { 0, &Ia_ret         },
  /* C4 */ { 0, &Ia_lesw_Gw_Mp  },
  /* C5 */ { 0, &Ia_ldsw_Gw_Mp  },
  /* C6 */ { 0, &Ia_movb_Eb_Ib  },
  /* C7 */ { 0, &Ia_movw_Ew_Iw  },
  /* C8 */ { 0, &Ia_enter       },
  /* C9 */ { 0, &Ia_leave       },
  /* CA */ { 0, &Ia_lret_Iw     },
  /* CB */ { 0, &Ia_lret        },
  /* CC */ { 0, &Ia_int3        },
  /* CD */ { 0, &Ia_int_Ib      },
  /* CE */ { 0, &Ia_into        },
  /* CF */ { 0, &Ia_iretw       },
  /* D0 */ { GRPN(G2EbI1)       },
  /* D1 */ { GRPN(G2EwI1)       },
  /* D2 */ { GRPN(G2EbCL)       },
  /* D3 */ { GRPN(G2EwCL)       },
  /* D4 */ { 0, &Ia_aam         },
  /* D5 */ { 0, &Ia_aad         },
  /* D6 */ { 0, &Ia_salc        },
  /* D7 */ { 0, &Ia_xlat        },
  /* D8 */ { GRPFP(D8)          },
  /* D9 */ { GRPFP(D9)          },
  /* DA */ { GRPFP(DA)          },
  /* DB */ { GRPFP(DB)          },
  /* DC */ { GRPFP(DC)          },
  /* DD */ { GRPFP(DD)          },
  /* DE */ { GRPFP(DE)          },
  /* DF */ { GRPFP(DF)          },
  /* E0 */ { 0, &Ia_loopne_Jb   },
  /* E1 */ { 0, &Ia_loope_Jb    },
  /* E2 */ { 0, &Ia_loop_Jb     },
  /* E3 */ { 0, &Ia_jcxz_Jb     },
  /* E4 */ { 0, &Ia_inb_AL_Ib   },
  /* E5 */ { 0, &Ia_inw_AX_Ib   },
  /* E6 */ { 0, &Ia_outb_Ib_AL  },
  /* E7 */ { 0, &Ia_outw_Ib_AX  },
  /* E8 */ { 0, &Ia_call_Jw     },
  /* E9 */ { 0, &Ia_jmp_Jw      },
  /* EA */ { 0, &Ia_ljmp_Apw    },
  /* EB */ { 0, &Ia_jmp_Jb      },
  /* EC */ { 0, &Ia_inb_AL_DX   },
  /* ED */ { 0, &Ia_inw_AX_DX   },
  /* EE */ { 0, &Ia_outb_DX_AL  },
  /* EF */ { 0, &Ia_outw_DX_AX  },
  /* F0 */ { 0, &Ia_prefix_lock },   // LOCK:
  /* F1 */ { 0, &Ia_int1        },
  /* F2 */ { 0, &Ia_prefix_repne },  // REPNE:
  /* F3 */ { 0, &Ia_prefix_rep  },   // REP:
  /* F4 */ { 0, &Ia_hlt         },
  /* F5 */ { 0, &Ia_cmc         },
  /* F6 */ { GRPN(G3Eb)         },
  /* F7 */ { GRPN(G3Ew)         },
  /* F8 */ { 0, &Ia_clc         },
  /* F9 */ { 0, &Ia_stc         },
  /* FA */ { 0, &Ia_cli         },
  /* FB */ { 0, &Ia_sti         },
  /* FC */ { 0, &Ia_cld         },
  /* FD */ { 0, &Ia_std         },
  /* FE */ { GRPN(G4)           },
  /* FF */ { GRPN(G5w)          },

  // 256 entries for two byte opcodes
  /* 0F 00 */ { GRPN(G6)          },
  /* 0F 01 */ { GRPN(G7)          },
  /* 0F 02 */ { 0, &Ia_larw_Gw_Ew },
  /* 0F 03 */ { 0, &Ia_lslw_Gw_Ew },
  /* 0F 04 */ { 0, &Ia_Invalid    },
  /* 0F 05 */ { 0, &Ia_syscall    },
  /* 0F 06 */ { 0, &Ia_clts       },
  /* 0F 07 */ { 0, &Ia_sysret     },
  /* 0F 08 */ { 0, &Ia_invd       },
  /* 0F 09 */ { 0, &Ia_wbinvd     },
  /* 0F 0A */ { 0, &Ia_Invalid    },
  /* 0F 0B */ { 0, &Ia_ud2a       },
  /* 0F 0C */ { 0, &Ia_Invalid    },
  /* 0F 0D */ { 0, &Ia_prefetch   },   // 3DNow!
  /* 0F 0E */ { 0, &Ia_femms      },   // 3DNow!
  /* 0F 0F */ { GRP3DNOW          },
  /* 0F 10 */ { GRPSSE(0f10)      },
  /* 0F 11 */ { GRPSSE(0f11)      },
  /* 0F 12 */ { GRPSSE(0f12)      },
  /* 0F 13 */ { GRPSSE(0f13)      },
  /* 0F 14 */ { GRPSSE(0f14)      },
  /* 0F 15 */ { GRPSSE(0f15)      },
  /* 0F 16 */ { GRPSSE(0f16)      },
  /* 0F 17 */ { GRPSSE(0f17)      },
  /* 0F 18 */ { GRPN(G16)         },
  /* 0F 19 */ { 0, &Ia_Invalid    },
  /* 0F 1A */ { 0, &Ia_Invalid    },
  /* 0F 1B */ { 0, &Ia_Invalid    },
  /* 0F 1C */ { 0, &Ia_Invalid    },
  /* 0F 1D */ { 0, &Ia_Invalid    },
  /* 0F 1E */ { 0, &Ia_Invalid    },
  /* 0F 1F */ { 0, &Ia_multibyte_nop },
  /* 0F 20 */ { 0, &Ia_movl_Rd_Cd },
  /* 0F 21 */ { 0, &Ia_movl_Rd_Dd },
  /* 0F 22 */ { 0, &Ia_movl_Cd_Rd },
  /* 0F 23 */ { 0, &Ia_movl_Dd_Rd },
  /* 0F 24 */ { 0, &Ia_movl_Rd_Td },
  /* 0F 25 */ { 0, &Ia_Invalid    },
  /* 0F 26 */ { 0, &Ia_movl_Td_Rd },
  /* 0F 27 */ { 0, &Ia_Invalid    },
  /* 0F 28 */ { GRPSSE(0f28) },
  /* 0F 29 */ { GRPSSE(0f29) },
  /* 0F 2A */ { GRPSSE(0f2a) },
  /* 0F 2B */ { GRPSSE(0f2b) },
  /* 0F 2C */ { GRPSSE(0f2c) },
  /* 0F 2D */ { GRPSSE(0f2d) },
  /* 0F 2E */ { GRPSSE(0f2e) },
  /* 0F 2F */ { GRPSSE(0f2f) },
  /* 0F 30 */ { 0, &Ia_wrmsr },
  /* 0F 31 */ { 0, &Ia_rdtsc },
  /* 0F 32 */ { 0, &Ia_rdmsr },
  /* 0F 33 */ { 0, &Ia_rdpmc },
  /* 0F 34 */ { 0, &Ia_sysenter },
  /* 0F 35 */ { 0, &Ia_sysexit  },
  /* 0F 36 */ { 0, &Ia_Invalid  },
  /* 0F 37 */ { 0, &Ia_Invalid  },
  /* 0F 38 */ { GR3BTAB(A4) },
  /* 0F 39 */ { 0, &Ia_Invalid  },
  /* 0F 3A */ { GR3BTAB(A5) },
  /* 0F 3B */ { 0, &Ia_Invalid  },
  /* 0F 3C */ { 0, &Ia_Invalid  },
  /* 0F 3D */ { 0, &Ia_Invalid  },
  /* 0F 3E */ { 0, &Ia_Invalid  },
  /* 0F 3F */ { 0, &Ia_Invalid  },
  /* 0F 40 */ { 0, &Ia_cmovow_Gw_Ew  },
  /* 0F 41 */ { 0, &Ia_cmovnow_Gw_Ew },
  /* 0F 42 */ { 0, &Ia_cmovcw_Gw_Ew  },
  /* 0F 43 */ { 0, &Ia_cmovncw_Gw_Ew },
  /* 0F 44 */ { 0, &Ia_cmovzw_Gw_Ew  },
  /* 0F 45 */ { 0, &Ia_cmovnzw_Gw_Ew },
  /* 0F 46 */ { 0, &Ia_cmovnaw_Gw_Ew },
  /* 0F 47 */ { 0, &Ia_cmovaw_Gw_Ew  },
  /* 0F 48 */ { 0, &Ia_cmovsw_Gw_Ew  },
  /* 0F 49 */ { 0, &Ia_cmovnsw_Gw_Ew },
  /* 0F 4A */ { 0, &Ia_cmovpw_Gw_Ew  },
  /* 0F 4B */ { 0, &Ia_cmovnpw_Gw_Ew },
  /* 0F 4C */ { 0, &Ia_cmovlw_Gw_Ew  },
  /* 0F 4D */ { 0, &Ia_cmovnlw_Gw_Ew },
  /* 0F 4E */ { 0, &Ia_cmovngw_Gw_Ew },
  /* 0F 4F */ { 0, &Ia_cmovgw_Gw_Ew  },
  /* 0F 50 */ { GRPSSE(0f50) },
  /* 0F 51 */ { GRPSSE(0f51) },
  /* 0F 52 */ { GRPSSE(0f52) },
  /* 0F 53 */ { GRPSSE(0f53) },
  /* 0F 54 */ { GRPSSE(0f54) },
  /* 0F 55 */ { GRPSSE(0f55) },
  /* 0F 56 */ { GRPSSE(0f56) },
  /* 0F 57 */ { GRPSSE(0f57) },
  /* 0F 58 */ { GRPSSE(0f58) },
  /* 0F 59 */ { GRPSSE(0f59) },
  /* 0F 5A */ { GRPSSE(0f5a) },
  /* 0F 5B */ { GRPSSE(0f5b) },
  /* 0F 5C */ { GRPSSE(0f5c) },
  /* 0F 5D */ { GRPSSE(0f5d) },
  /* 0F 5E */ { GRPSSE(0f5e) },
  /* 0F 5F */ { GRPSSE(0f5f) },
  /* 0F 60 */ { GRPSSE(0f60) },
  /* 0F 61 */ { GRPSSE(0f61) },
  /* 0F 62 */ { GRPSSE(0f62) },
  /* 0F 63 */ { GRPSSE(0f63) },
  /* 0F 64 */ { GRPSSE(0f64) },
  /* 0F 65 */ { GRPSSE(0f65) },
  /* 0F 66 */ { GRPSSE(0f66) },
  /* 0F 67 */ { GRPSSE(0f67) },
  /* 0F 68 */ { GRPSSE(0f68) },
  /* 0F 69 */ { GRPSSE(0f69) },
  /* 0F 6A */ { GRPSSE(0f6a) },
  /* 0F 6B */ { GRPSSE(0f6b) },
  /* 0F 6C */ { GRPSSE(0f6c) },
  /* 0F 6D */ { GRPSSE(0f6d) },
  /* 0F 6E */ { GRPSSE(0f6e) },
  /* 0F 6F */ { GRPSSE(0f6f) },
  /* 0F 70 */ { GRPSSE(0f70) },
  /* 0F 71 */ { GRPN(G12)    },
  /* 0F 72 */ { GRPN(G13)    },
  /* 0F 73 */ { GRPN(G14)    },
  /* 0F 74 */ { GRPSSE(0f74) },
  /* 0F 75 */ { GRPSSE(0f75) },
  /* 0F 76 */ { GRPSSE(0f76) },
  /* 0F 77 */ { 0, &Ia_emms  },
  /* 0F 78 */ { 0, &Ia_Invalid },
  /* 0F 79 */ { 0, &Ia_Invalid },
  /* 0F 7A */ { 0, &Ia_Invalid },
  /* 0F 7B */ { 0, &Ia_Invalid },
  /* 0F 7C */ { GRPSSE(0f7c) },
  /* 0F 7D */ { GRPSSE(0f7d) },
  /* 0F 7E */ { GRPSSE(0f7e) },
  /* 0F 7F */ { GRPSSE(0f7f) },
  /* 0F 80 */ { 0, &Ia_jo_Jw     },
  /* 0F 81 */ { 0, &Ia_jno_Jw    },
  /* 0F 82 */ { 0, &Ia_jb_Jw     },
  /* 0F 83 */ { 0, &Ia_jnb_Jw    },
  /* 0F 84 */ { 0, &Ia_jz_Jw     },
  /* 0F 85 */ { 0, &Ia_jnz_Jw    },
  /* 0F 86 */ { 0, &Ia_jbe_Jw    },
  /* 0F 87 */ { 0, &Ia_jnbe_Jw   },
  /* 0F 88 */ { 0, &Ia_js_Jw     },
  /* 0F 89 */ { 0, &Ia_jns_Jw    },
  /* 0F 8A */ { 0, &Ia_jp_Jw     },
  /* 0F 8B */ { 0, &Ia_jnp_Jw    },
  /* 0F 8C */ { 0, &Ia_jl_Jw     },
  /* 0F 8D */ { 0, &Ia_jnl_Jw    },
  /* 0F 8E */ { 0, &Ia_jle_Jw    },
  /* 0F 8F */ { 0, &Ia_jnle_Jw   },
  /* 0F 90 */ { 0, &Ia_seto_Eb   },
  /* 0F 91 */ { 0, &Ia_setno_Eb  },
  /* 0F 92 */ { 0, &Ia_setb_Eb   },
  /* 0F 93 */ { 0, &Ia_setnb_Eb  },
  /* 0F 94 */ { 0, &Ia_setz_Eb   },
  /* 0F 95 */ { 0, &Ia_setnz_Eb  },
  /* 0F 96 */ { 0, &Ia_setbe_Eb  },
  /* 0F 97 */ { 0, &Ia_setnbe_Eb },
  /* 0F 98 */ { 0, &Ia_sets_Eb   },
  /* 0F 99 */ { 0, &Ia_setns_Eb  },
  /* 0F 9A */ { 0, &Ia_setp_Eb   },
  /* 0F 9B */ { 0, &Ia_setnp_Eb  },
  /* 0F 9C */ { 0, &Ia_setl_Eb   },
  /* 0F 9D */ { 0, &Ia_setnl_Eb  },
  /* 0F 9E */ { 0, &Ia_setle_Eb  },
  /* 0F 9F */ { 0, &Ia_setnle_Eb },
  /* 0F A0 */ { 0, &Ia_pushw_FS  },
  /* 0F A1 */ { 0, &Ia_popw_FS   },
  /* 0F A2 */ { 0, &Ia_cpuid     },
  /* 0F A3 */ { 0, &Ia_btw_Ew_Gw },
  /* 0F A4 */ { 0, &Ia_shldw_Ew_Gw_Ib },
  /* 0F A5 */ { 0, &Ia_shldw_Ew_Gw_CL },
  /* 0F A6 */ { 0, &Ia_Invalid        },
  /* 0F A7 */ { 0, &Ia_Invalid        },
  /* 0F A8 */ { 0, &Ia_pushw_GS       },
  /* 0F A9 */ { 0, &Ia_popw_GS        },
  /* 0F AA */ { 0, &Ia_rsm            },
  /* 0F AB */ { 0, &Ia_btsw_Ew_Gw     },
  /* 0F AC */ { 0, &Ia_shrdw_Ew_Gw_Ib },
  /* 0F AD */ { 0, &Ia_shrdw_Ew_Gw_CL },
  /* 0F AE */ { GRPN(G15)             },
  /* 0F AF */ { 0, &Ia_imulw_Gw_Ew    },
  /* 0F B0 */ { 0, &Ia_cmpxchgb_Eb_Gb },
  /* 0F B1 */ { 0, &Ia_cmpxchgw_Ew_Gw },
  /* 0F B2 */ { 0, &Ia_lssw_Gw_Mp     },
  /* 0F B3 */ { 0, &Ia_btrw_Ew_Gw     },
  /* 0F B4 */ { 0, &Ia_lfsw_Gw_Mp     },
  /* 0F B5 */ { 0, &Ia_lgsw_Gw_Mp     },
  /* 0F B6 */ { 0, &Ia_movzbw_Gw_Eb   },
  /* 0F B7 */ { 0, &Ia_movw_Gw_Ew     },
  /* 0F B8 */ { 0, &Ia_Invalid        },
  /* 0F B9 */ { 0, &Ia_ud2b           },
  /* 0F BA */ { GRPN(G8EwIb)          },
  /* 0F BB */ { 0, &Ia_btcw_Ew_Gw     },
  /* 0F BC */ { 0, &Ia_bsfw_Gw_Ew     },
  /* 0F BD */ { 0, &Ia_bsrw_Gw_Ew     },
  /* 0F BE */ { 0, &Ia_movsbw_Gw_Eb   },
  /* 0F BF */ { 0, &Ia_movw_Gw_Ew     },
  /* 0F C0 */ { 0, &Ia_xaddb_Eb_Gb    },
  /* 0F C0 */ { 0, &Ia_xaddw_Ew_Gw    },
  /* 0F C2 */ { GRPSSE(0fc2) },
  /* 0F C3 */ { GRPSSE(0fc3) },
  /* 0F C4 */ { GRPSSE(0fc4) },
  /* 0F C5 */ { GRPSSE(0fc5) },
  /* 0F C6 */ { GRPSSE(0fc6) },
  /* 0F C7 */ { GRPN(G9)     },
  /* 0F C8 */ { 0, &Ia_bswapl_ERX },
  /* 0F C9 */ { 0, &Ia_bswapl_ERX },
  /* 0F CA */ { 0, &Ia_bswapl_ERX },
  /* 0F CB */ { 0, &Ia_bswapl_ERX },
  /* 0F CC */ { 0, &Ia_bswapl_ERX },
  /* 0F CD */ { 0, &Ia_bswapl_ERX },
  /* 0F CE */ { 0, &Ia_bswapl_ERX },
  /* 0F CF */ { 0, &Ia_bswapl_ERX },
  /* 0F D0 */ { GRPSSE(0fd0) },
  /* 0F D1 */ { GRPSSE(0fd1) },
  /* 0F D2 */ { GRPSSE(0fd2) },
  /* 0F D3 */ { GRPSSE(0fd3) },
  /* 0F D4 */ { GRPSSE(0fd4) },
  /* 0F D5 */ { GRPSSE(0fd5) },
  /* 0F D6 */ { GRPSSE(0fd6) },
  /* 0F D7 */ { GRPSSE(0fd7) },
  /* 0F D8 */ { GRPSSE(0fd8) },
  /* 0F D9 */ { GRPSSE(0fd9) },
  /* 0F DA */ { GRPSSE(0fda) },
  /* 0F DB */ { GRPSSE(0fdb) },
  /* 0F DC */ { GRPSSE(0fdc) },
  /* 0F DD */ { GRPSSE(0fdd) },
  /* 0F DE */ { GRPSSE(0fde) },
  /* 0F DF */ { GRPSSE(0fdf) },
  /* 0F E0 */ { GRPSSE(0fe0) },
  /* 0F E1 */ { GRPSSE(0fe1) },
  /* 0F E2 */ { GRPSSE(0fe2) },
  /* 0F E3 */ { GRPSSE(0fe3) },
  /* 0F E4 */ { GRPSSE(0fe4) },
  /* 0F E5 */ { GRPSSE(0fe5) },
  /* 0F E6 */ { GRPSSE(0fe6) },
  /* 0F E7 */ { GRPSSE(0fe7) },
  /* 0F E8 */ { GRPSSE(0fe8) },
  /* 0F E9 */ { GRPSSE(0fe9) },
  /* 0F EA */ { GRPSSE(0fea) },
  /* 0F EB */ { GRPSSE(0feb) },
  /* 0F EC */ { GRPSSE(0fec) },
  /* 0F ED */ { GRPSSE(0fed) },
  /* 0F EE */ { GRPSSE(0fee) },
  /* 0F EF */ { GRPSSE(0fef) },
  /* 0F F0 */ { GRPSSE(0ff0) },
  /* 0F F1 */ { GRPSSE(0ff1) },
  /* 0F F2 */ { GRPSSE(0ff2) },
  /* 0F F3 */ { GRPSSE(0ff3) },
  /* 0F F4 */ { GRPSSE(0ff4) },
  /* 0F F5 */ { GRPSSE(0ff5) },
  /* 0F F6 */ { GRPSSE(0ff6) },
  /* 0F F7 */ { GRPSSE(0ff7) },
  /* 0F F8 */ { GRPSSE(0ff8) },
  /* 0F F9 */ { GRPSSE(0ff9) },
  /* 0F FA */ { GRPSSE(0ffa) },
  /* 0F FB */ { GRPSSE(0ffb) },
  /* 0F FC */ { GRPSSE(0ffc) },
  /* 0F FD */ { GRPSSE(0ffd) },
  /* 0F FE */ { GRPSSE(0ffe) },
  /* 0F FF */ { 0, &Ia_Invalid }
};

/* ************************************************************************ */
/* 32-bit operand size */

static BxDisasmOpcodeTable_t BxDisasmOpcodes32[256*2] = {
  // 256 entries for single byte opcodes
  /* 00 */ { 0, &Ia_addb_Eb_Gb    },
  /* 01 */ { 0, &Ia_addl_Ed_Gd    },
  /* 02 */ { 0, &Ia_addb_Gb_Eb    },
  /* 03 */ { 0, &Ia_addl_Gd_Ed    },
  /* 04 */ { 0, &Ia_addb_AL_Ib,   },
  /* 05 */ { 0, &Ia_addl_EAX_Id,  },
  /* 06 */ { 0, &Ia_pushl_ES      },
  /* 07 */ { 0, &Ia_popl_ES       },
  /* 08 */ { 0, &Ia_orb_Eb_Gb     },
  /* 09 */ { 0, &Ia_orl_Ed_Gd     },
  /* 0A */ { 0, &Ia_orb_Gb_Eb     },
  /* 0B */ { 0, &Ia_orl_Gd_Ed     },
  /* 0C */ { 0, &Ia_orb_AL_Ib     },
  /* 0D */ { 0, &Ia_orl_EAX_Id    },
  /* 0E */ { 0, &Ia_pushl_CS      },
  /* 0F */ { 0, &Ia_error         },   // 2 byte escape
  /* 10 */ { 0, &Ia_adcb_Eb_Gb    },
  /* 11 */ { 0, &Ia_adcl_Ed_Gd    },
  /* 12 */ { 0, &Ia_adcb_Gb_Eb    },
  /* 13 */ { 0, &Ia_adcl_Gd_Ed    },
  /* 14 */ { 0, &Ia_adcb_AL_Ib    },
  /* 15 */ { 0, &Ia_adcl_EAX_Id   },
  /* 16 */ { 0, &Ia_pushl_SS      },
  /* 17 */ { 0, &Ia_popl_SS       },
  /* 18 */ { 0, &Ia_sbbb_Eb_Gb    },
  /* 19 */ { 0, &Ia_sbbl_Ed_Gd    },
  /* 1A */ { 0, &Ia_sbbb_Gb_Eb    },
  /* 1B */ { 0, &Ia_sbbl_Gd_Ed    },
  /* 1C */ { 0, &Ia_sbbb_AL_Ib    },
  /* 1D */ { 0, &Ia_sbbl_EAX_Id   },
  /* 1E */ { 0, &Ia_pushl_DS      },
  /* 1F */ { 0, &Ia_popl_DS       },
  /* 20 */ { 0, &Ia_andb_Eb_Gb    },
  /* 21 */ { 0, &Ia_andl_Ed_Gd    },
  /* 22 */ { 0, &Ia_andb_Gb_Eb    },
  /* 23 */ { 0, &Ia_andl_Gd_Ed    },
  /* 24 */ { 0, &Ia_andb_AL_Ib    },
  /* 25 */ { 0, &Ia_andl_EAX_Id   },
  /* 26 */ { 0, &Ia_prefix_es     },   // ES:
  /* 27 */ { 0, &Ia_daa           },
  /* 28 */ { 0, &Ia_subb_Eb_Gb    },
  /* 29 */ { 0, &Ia_subl_Ed_Gd    },
  /* 2A */ { 0, &Ia_subb_Gb_Eb    },
  /* 2B */ { 0, &Ia_subl_Gd_Ed    },
  /* 2C */ { 0, &Ia_subb_AL_Ib    },
  /* 2D */ { 0, &Ia_subl_EAX_Id   },
  /* 2E */ { 0, &Ia_prefix_cs     },   // CS:
  /* 2F */ { 0, &Ia_das           },
  /* 30 */ { 0, &Ia_xorb_Eb_Gb    },
  /* 31 */ { 0, &Ia_xorl_Ed_Gd    },
  /* 32 */ { 0, &Ia_xorb_Gb_Eb    },
  /* 33 */ { 0, &Ia_xorl_Gd_Ed    },
  /* 34 */ { 0, &Ia_xorb_AL_Ib    },
  /* 35 */ { 0, &Ia_xorl_EAX_Id   },
  /* 36 */ { 0, &Ia_prefix_ss     },   // SS:
  /* 37 */ { 0, &Ia_aaa           },
  /* 38 */ { 0, &Ia_cmpb_Eb_Gb    },
  /* 39 */ { 0, &Ia_cmpl_Ed_Gd    },
  /* 3A */ { 0, &Ia_cmpb_Gb_Eb    },
  /* 3B */ { 0, &Ia_cmpl_Gd_Ed    },
  /* 3C */ { 0, &Ia_cmpb_AL_Ib    },
  /* 3D */ { 0, &Ia_cmpl_EAX_Id   },
  /* 3E */ { 0, &Ia_prefix_ds     },   // DS:
  /* 3F */ { 0, &Ia_aas           },
  /* 40 */ { 0, &Ia_incl_ERX      },
  /* 41 */ { 0, &Ia_incl_ERX      },
  /* 42 */ { 0, &Ia_incl_ERX      },
  /* 43 */ { 0, &Ia_incl_ERX      },
  /* 44 */ { 0, &Ia_incl_ERX      },
  /* 45 */ { 0, &Ia_incl_ERX      },
  /* 46 */ { 0, &Ia_incl_ERX      },
  /* 47 */ { 0, &Ia_incl_ERX      },
  /* 48 */ { 0, &Ia_decl_ERX      },
  /* 49 */ { 0, &Ia_decl_ERX      },
  /* 4A */ { 0, &Ia_decl_ERX      },
  /* 4B */ { 0, &Ia_decl_ERX      },
  /* 4C */ { 0, &Ia_decl_ERX      },
  /* 4D */ { 0, &Ia_decl_ERX      },
  /* 4E */ { 0, &Ia_decl_ERX      },
  /* 4F */ { 0, &Ia_decl_ERX      },
  /* 50 */ { 0, &Ia_pushl_ERX     },
  /* 51 */ { 0, &Ia_pushl_ERX     },
  /* 52 */ { 0, &Ia_pushl_ERX     },
  /* 53 */ { 0, &Ia_pushl_ERX     },
  /* 54 */ { 0, &Ia_pushl_ERX     },
  /* 55 */ { 0, &Ia_pushl_ERX     },
  /* 56 */ { 0, &Ia_pushl_ERX     },
  /* 57 */ { 0, &Ia_pushl_ERX     },
  /* 58 */ { 0, &Ia_popl_ERX      },
  /* 59 */ { 0, &Ia_popl_ERX      },
  /* 5A */ { 0, &Ia_popl_ERX      },
  /* 5B */ { 0, &Ia_popl_ERX      },
  /* 5C */ { 0, &Ia_popl_ERX      },
  /* 5D */ { 0, &Ia_popl_ERX      },
  /* 5E */ { 0, &Ia_popl_ERX      },
  /* 5F */ { 0, &Ia_popl_ERX      },
  /* 60 */ { 0, &Ia_pushal        },
  /* 61 */ { 0, &Ia_popal         },
  /* 62 */ { 0, &Ia_boundl_Gd_Ma  },
  /* 63 */ { 0, &Ia_arpl_Ew_Rw    },
  /* 64 */ { 0, &Ia_prefix_fs     },   // FS:
  /* 65 */ { 0, &Ia_prefix_gs     },   // GS:
  /* 66 */ { 0, &Ia_prefix_osize  },   // OSIZE:
  /* 67 */ { 0, &Ia_prefix_asize  },   // ASIZE:
  /* 68 */ { 0, &Ia_pushl_Id      },
  /* 69 */ { 0, &Ia_imull_Gd_Ed_Id  },
  /* 6A */ { 0, &Ia_pushl_sIb     },
  /* 6B */ { 0, &Ia_imull_Gd_Ed_sIb },
  /* 6C */ { 0, &Ia_insb_Yb_DX    },
  /* 6D */ { 0, &Ia_insl_Yd_DX    },
  /* 6E */ { 0, &Ia_outsb_DX_Xb   },
  /* 6F */ { 0, &Ia_outsl_DX_Xd   },
  /* 70 */ { 0, &Ia_jo_Jb         },
  /* 71 */ { 0, &Ia_jno_Jb        },
  /* 72 */ { 0, &Ia_jb_Jb         },
  /* 73 */ { 0, &Ia_jnb_Jb        },
  /* 74 */ { 0, &Ia_jz_Jb         },
  /* 75 */ { 0, &Ia_jnz_Jb        },
  /* 76 */ { 0, &Ia_jbe_Jb        },
  /* 77 */ { 0, &Ia_jnbe_Jb       },
  /* 78 */ { 0, &Ia_js_Jb         },
  /* 79 */ { 0, &Ia_jns_Jb        },
  /* 7A */ { 0, &Ia_jp_Jb         },
  /* 7B */ { 0, &Ia_jnp_Jb        },
  /* 7C */ { 0, &Ia_jl_Jb         },
  /* 7D */ { 0, &Ia_jnl_Jb        },
  /* 7E */ { 0, &Ia_jle_Jb        },
  /* 7F */ { 0, &Ia_jnle_Jb       },
  /* 80 */ { GRPN(G1EbIb)         },
  /* 81 */ { GRPN(G1EdId)         },
  /* 82 */ { GRPN(G1EbIb)         },
  /* 83 */ { GRPN(G1EdIb)         },
  /* 84 */ { 0, &Ia_testb_Eb_Gb   },
  /* 85 */ { 0, &Ia_testl_Ed_Gd   },
  /* 86 */ { 0, &Ia_xchgb_Eb_Gb   },
  /* 87 */ { 0, &Ia_xchgl_Ed_Gd   },
  /* 88 */ { 0, &Ia_movb_Eb_Gb    },
  /* 89 */ { 0, &Ia_movl_Ed_Gd    },
  /* 8A */ { 0, &Ia_movb_Gb_Eb    },
  /* 8B */ { 0, &Ia_movl_Gd_Ed    },
  /* 8C */ { 0, &Ia_movw_Ew_Sw    },
  /* 8D */ { 0, &Ia_leal_Gd_Md    },
  /* 8E */ { 0, &Ia_movw_Sw_Ew    },
  /* 8F */ { 0, &Ia_popl_Ed       },
  /* 90 */ { 0, &Ia_nop           },
  /* 91 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 92 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 93 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 94 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 95 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 96 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 97 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 98 */ { 0, &Ia_cwde          },
  /* 99 */ { 0, &Ia_cdq           },
  /* 9A */ { 0, &Ia_lcall_Apd     },
  /* 9B */ { 0, &Ia_fwait         },
  /* 9C */ { 0, &Ia_pushfl        },
  /* 9D */ { 0, &Ia_popfl         },
  /* 9E */ { 0, &Ia_sahf          },
  /* 9F */ { 0, &Ia_lahf          },
  /* A0 */ { 0, &Ia_movb_AL_Ob    },
  /* A1 */ { 0, &Ia_movl_EAX_Od   },
  /* A0 */ { 0, &Ia_movb_Ob_AL    },
  /* A1 */ { 0, &Ia_movl_Od_EAX   },
  /* A4 */ { 0, &Ia_movsb_Yb_Xb   },
  /* A5 */ { 0, &Ia_movsl_Yd_Xd   },
  /* A6 */ { 0, &Ia_cmpsb_Yb_Xb   },
  /* A7 */ { 0, &Ia_cmpsl_Yd_Xd   },
  /* A8 */ { 0, &Ia_testb_AL_Ib   },
  /* A9 */ { 0, &Ia_testl_EAX_Id  },
  /* AA */ { 0, &Ia_stosb_Yb_AL   },
  /* AB */ { 0, &Ia_stosl_Yd_EAX  },
  /* AC */ { 0, &Ia_lodsb_AL_Xb   },
  /* AD */ { 0, &Ia_lodsl_EAX_Xd  },
  /* AE */ { 0, &Ia_scasb_Yb_AL   },
  /* AF */ { 0, &Ia_scasl_Yd_EAX  },
  /* B0 */ { 0, &Ia_movb_R8_Ib    },
  /* B1 */ { 0, &Ia_movb_R8_Ib    },
  /* B2 */ { 0, &Ia_movb_R8_Ib    },
  /* B3 */ { 0, &Ia_movb_R8_Ib    },
  /* B4 */ { 0, &Ia_movb_R8_Ib    },
  /* B5 */ { 0, &Ia_movb_R8_Ib    },
  /* B6 */ { 0, &Ia_movb_R8_Ib    },
  /* B7 */ { 0, &Ia_movb_R8_Ib    },
  /* B8 */ { 0, &Ia_movl_ERX_Id   },
  /* B9 */ { 0, &Ia_movl_ERX_Id   },
  /* BA */ { 0, &Ia_movl_ERX_Id   },
  /* BB */ { 0, &Ia_movl_ERX_Id   },
  /* BC */ { 0, &Ia_movl_ERX_Id   },
  /* BD */ { 0, &Ia_movl_ERX_Id   },
  /* BE */ { 0, &Ia_movl_ERX_Id   },
  /* BF */ { 0, &Ia_movl_ERX_Id   },
  /* C0 */ { GRPN(G2Eb)           },
  /* C1 */ { GRPN(G2Ed)           },
  /* C2 */ { 0, &Ia_ret_Iw        },
  /* C3 */ { 0, &Ia_ret           },
  /* C4 */ { 0, &Ia_lesl_Gd_Mp    },
  /* C5 */ { 0, &Ia_ldsl_Gd_Mp    },
  /* C6 */ { 0, &Ia_movb_Eb_Ib    },
  /* C7 */ { 0, &Ia_movl_Ed_Id    },
  /* C8 */ { 0, &Ia_enter         },
  /* C9 */ { 0, &Ia_leave         },
  /* CA */ { 0, &Ia_lret_Iw       },
  /* CB */ { 0, &Ia_lret          },
  /* CC */ { 0, &Ia_int3          },
  /* CD */ { 0, &Ia_int_Ib        },
  /* CE */ { 0, &Ia_into          },
  /* CF */ { 0, &Ia_iretl         },
  /* D0 */ { GRPN(G2EbI1)         },
  /* D1 */ { GRPN(G2EdI1)         },
  /* D2 */ { GRPN(G2EbCL)         },
  /* D3 */ { GRPN(G2EdCL)         },
  /* D4 */ { 0, &Ia_aam           },
  /* D5 */ { 0, &Ia_aad           },
  /* D6 */ { 0, &Ia_salc          },
  /* D7 */ { 0, &Ia_xlat          },
  /* D8 */ { GRPFP(D8)            },
  /* D9 */ { GRPFP(D9)            },
  /* DA */ { GRPFP(DA)            },
  /* DB */ { GRPFP(DB)            },
  /* DC */ { GRPFP(DC)            },
  /* DD */ { GRPFP(DD)            },
  /* DE */ { GRPFP(DE)            },
  /* DF */ { GRPFP(DF)            },
  /* E0 */ { 0, &Ia_loopne_Jb     },
  /* E1 */ { 0, &Ia_loope_Jb      },
  /* E2 */ { 0, &Ia_loop_Jb       },
  /* E3 */ { 0, &Ia_jcxz_Jb       },
  /* E4 */ { 0, &Ia_inb_AL_Ib     },
  /* E5 */ { 0, &Ia_inl_EAX_Ib    },
  /* E6 */ { 0, &Ia_outb_Ib_AL    },
  /* E7 */ { 0, &Ia_outl_Ib_EAX   },
  /* E8 */ { 0, &Ia_call_Jd       },
  /* E9 */ { 0, &Ia_jmp_Jd        },
  /* EA */ { 0, &Ia_ljmp_Apd      },
  /* EB */ { 0, &Ia_jmp_Jb        },
  /* EC */ { 0, &Ia_inb_AL_DX     },
  /* ED */ { 0, &Ia_inl_EAX_DX    },
  /* EE */ { 0, &Ia_outb_DX_AL    },
  /* EF */ { 0, &Ia_outl_DX_EAX   },
  /* F0 */ { 0, &Ia_prefix_lock   },   // LOCK:
  /* F1 */ { 0, &Ia_int1          },
  /* F2 */ { 0, &Ia_prefix_repne  },   // REPNE:
  /* F3 */ { 0, &Ia_prefix_rep    },   // REP:
  /* F4 */ { 0, &Ia_hlt           },
  /* F5 */ { 0, &Ia_cmc           },
  /* F6 */ { GRPN(G3Eb)           },
  /* F7 */ { GRPN(G3Ed)           },
  /* F8 */ { 0, &Ia_clc           },
  /* F9 */ { 0, &Ia_stc           },
  /* FA */ { 0, &Ia_cli           },
  /* FB */ { 0, &Ia_sti           },
  /* FC */ { 0, &Ia_cld           },
  /* FD */ { 0, &Ia_std           },
  /* FE */ { GRPN(G4)             },
  /* FF */ { GRPN(G5d)            },

  // 256 entries for two byte opcodes
  /* 0F 00 */ { GRPN(G6)          },
  /* 0F 01 */ { GRPN(G7)          },
  /* 0F 02 */ { 0, &Ia_larl_Gd_Ew },
  /* 0F 03 */ { 0, &Ia_lsll_Gd_Ew },
  /* 0F 04 */ { 0, &Ia_Invalid    },
  /* 0F 05 */ { 0, &Ia_syscall    },
  /* 0F 06 */ { 0, &Ia_clts       },
  /* 0F 07 */ { 0, &Ia_sysret     },
  /* 0F 08 */ { 0, &Ia_invd       },
  /* 0F 09 */ { 0, &Ia_wbinvd     },
  /* 0F 0A */ { 0, &Ia_Invalid    },
  /* 0F 0B */ { 0, &Ia_ud2a       },
  /* 0F 0C */ { 0, &Ia_Invalid    },
  /* 0F 0D */ { 0, &Ia_prefetch   },   // 3DNow!
  /* 0F 0E */ { 0, &Ia_femms      },   // 3DNow!
  /* 0F 0F */ { GRP3DNOW          },
  /* 0F 10 */ { GRPSSE(0f10)      },
  /* 0F 11 */ { GRPSSE(0f11)      },
  /* 0F 12 */ { GRPSSE(0f12)      },
  /* 0F 13 */ { GRPSSE(0f13)      },
  /* 0F 14 */ { GRPSSE(0f14)      },
  /* 0F 15 */ { GRPSSE(0f15)      },
  /* 0F 16 */ { GRPSSE(0f16)      },
  /* 0F 17 */ { GRPSSE(0f17)      },
  /* 0F 18 */ { GRPN(G16)         },
  /* 0F 19 */ { 0, &Ia_Invalid    },
  /* 0F 1A */ { 0, &Ia_Invalid    },
  /* 0F 1B */ { 0, &Ia_Invalid    },
  /* 0F 1C */ { 0, &Ia_Invalid    },
  /* 0F 1D */ { 0, &Ia_Invalid    },
  /* 0F 1E */ { 0, &Ia_Invalid    },
  /* 0F 1F */ { 0, &Ia_multibyte_nop },
  /* 0F 20 */ { 0, &Ia_movl_Rd_Cd },
  /* 0F 21 */ { 0, &Ia_movl_Rd_Dd },
  /* 0F 22 */ { 0, &Ia_movl_Cd_Rd },
  /* 0F 23 */ { 0, &Ia_movl_Dd_Rd },
  /* 0F 24 */ { 0, &Ia_movl_Rd_Td },
  /* 0F 25 */ { 0, &Ia_Invalid    },
  /* 0F 26 */ { 0, &Ia_movl_Td_Rd },
  /* 0F 27 */ { 0, &Ia_Invalid    },
  /* 0F 28 */ { GRPSSE(0f28) },
  /* 0F 29 */ { GRPSSE(0f29) },
  /* 0F 2A */ { GRPSSE(0f2a) },
  /* 0F 2B */ { GRPSSE(0f2b) },
  /* 0F 2C */ { GRPSSE(0f2c) },
  /* 0F 2D */ { GRPSSE(0f2d) },
  /* 0F 2E */ { GRPSSE(0f2e) },
  /* 0F 2F */ { GRPSSE(0f2f) },
  /* 0F 30 */ { 0, &Ia_wrmsr },
  /* 0F 31 */ { 0, &Ia_rdtsc },
  /* 0F 32 */ { 0, &Ia_rdmsr },
  /* 0F 33 */ { 0, &Ia_rdpmc },
  /* 0F 34 */ { 0, &Ia_sysenter },
  /* 0F 35 */ { 0, &Ia_sysexit  },
  /* 0F 36 */ { 0, &Ia_Invalid  },
  /* 0F 37 */ { 0, &Ia_Invalid  },
  /* 0F 38 */ { GR3BTAB(A4) },
  /* 0F 39 */ { 0, &Ia_Invalid  },
  /* 0F 3A */ { GR3BTAB(A5) },
  /* 0F 3B */ { 0, &Ia_Invalid  },
  /* 0F 3C */ { 0, &Ia_Invalid  },
  /* 0F 3D */ { 0, &Ia_Invalid  },
  /* 0F 3E */ { 0, &Ia_Invalid  },
  /* 0F 3F */ { 0, &Ia_Invalid  },
  /* 0F 40 */ { 0, &Ia_cmovol_Gd_Ed  },
  /* 0F 41 */ { 0, &Ia_cmovnol_Gd_Ed },
  /* 0F 42 */ { 0, &Ia_cmovcl_Gd_Ed  },
  /* 0F 43 */ { 0, &Ia_cmovncl_Gd_Ed },
  /* 0F 44 */ { 0, &Ia_cmovzl_Gd_Ed  },
  /* 0F 45 */ { 0, &Ia_cmovnzl_Gd_Ed },
  /* 0F 46 */ { 0, &Ia_cmovnal_Gd_Ed },
  /* 0F 47 */ { 0, &Ia_cmoval_Gd_Ed  },
  /* 0F 48 */ { 0, &Ia_cmovsl_Gd_Ed  },
  /* 0F 49 */ { 0, &Ia_cmovnsl_Gd_Ed },
  /* 0F 4A */ { 0, &Ia_cmovpl_Gd_Ed  },
  /* 0F 4B */ { 0, &Ia_cmovnpl_Gd_Ed },
  /* 0F 4C */ { 0, &Ia_cmovll_Gd_Ed  },
  /* 0F 4D */ { 0, &Ia_cmovnll_Gd_Ed },
  /* 0F 4E */ { 0, &Ia_cmovngl_Gd_Ed },
  /* 0F 4F */ { 0, &Ia_cmovgl_Gd_Ed  },
  /* 0F 50 */ { GRPSSE(0f50) },
  /* 0F 51 */ { GRPSSE(0f51) },
  /* 0F 52 */ { GRPSSE(0f52) },
  /* 0F 53 */ { GRPSSE(0f53) },
  /* 0F 54 */ { GRPSSE(0f54) },
  /* 0F 55 */ { GRPSSE(0f55) },
  /* 0F 56 */ { GRPSSE(0f56) },
  /* 0F 57 */ { GRPSSE(0f57) },
  /* 0F 58 */ { GRPSSE(0f58) },
  /* 0F 59 */ { GRPSSE(0f59) },
  /* 0F 5A */ { GRPSSE(0f5a) },
  /* 0F 5B */ { GRPSSE(0f5b) },
  /* 0F 5C */ { GRPSSE(0f5c) },
  /* 0F 5D */ { GRPSSE(0f5d) },
  /* 0F 5E */ { GRPSSE(0f5e) },
  /* 0F 5F */ { GRPSSE(0f5f) },
  /* 0F 60 */ { GRPSSE(0f60) },
  /* 0F 61 */ { GRPSSE(0f61) },
  /* 0F 62 */ { GRPSSE(0f62) },
  /* 0F 63 */ { GRPSSE(0f63) },
  /* 0F 64 */ { GRPSSE(0f64) },
  /* 0F 65 */ { GRPSSE(0f65) },
  /* 0F 66 */ { GRPSSE(0f66) },
  /* 0F 67 */ { GRPSSE(0f67) },
  /* 0F 68 */ { GRPSSE(0f68) },
  /* 0F 69 */ { GRPSSE(0f69) },
  /* 0F 6A */ { GRPSSE(0f6a) },
  /* 0F 6B */ { GRPSSE(0f6b) },
  /* 0F 6C */ { GRPSSE(0f6c) },
  /* 0F 6D */ { GRPSSE(0f6d) },
  /* 0F 6E */ { GRPSSE(0f6e) },
  /* 0F 6F */ { GRPSSE(0f6f) },
  /* 0F 70 */ { GRPSSE(0f70) },
  /* 0F 71 */ { GRPN(G12)    },
  /* 0F 72 */ { GRPN(G13)    },
  /* 0F 73 */ { GRPN(G14)    },
  /* 0F 74 */ { GRPSSE(0f74) },
  /* 0F 75 */ { GRPSSE(0f75) },
  /* 0F 76 */ { GRPSSE(0f76) },
  /* 0F 77 */ { 0, &Ia_emms  },
  /* 0F 78 */ { 0, &Ia_Invalid },
  /* 0F 79 */ { 0, &Ia_Invalid },
  /* 0F 7A */ { 0, &Ia_Invalid },
  /* 0F 7B */ { 0, &Ia_Invalid },
  /* 0F 7C */ { GRPSSE(0f7c) },
  /* 0F 7D */ { GRPSSE(0f7d) },
  /* 0F 7E */ { GRPSSE(0f7e) },
  /* 0F 7F */ { GRPSSE(0f7f) },
  /* 0F 80 */ { 0, &Ia_jo_Jd     },
  /* 0F 81 */ { 0, &Ia_jno_Jd    },
  /* 0F 82 */ { 0, &Ia_jb_Jd     },
  /* 0F 83 */ { 0, &Ia_jnb_Jd    },
  /* 0F 84 */ { 0, &Ia_jz_Jd     },
  /* 0F 85 */ { 0, &Ia_jnz_Jd    },
  /* 0F 86 */ { 0, &Ia_jbe_Jd    },
  /* 0F 87 */ { 0, &Ia_jnbe_Jd   },
  /* 0F 88 */ { 0, &Ia_js_Jd     },
  /* 0F 89 */ { 0, &Ia_jns_Jd    },
  /* 0F 8A */ { 0, &Ia_jp_Jd     },
  /* 0F 8B */ { 0, &Ia_jnp_Jd    },
  /* 0F 8C */ { 0, &Ia_jl_Jd     },
  /* 0F 8D */ { 0, &Ia_jnl_Jd    },
  /* 0F 8E */ { 0, &Ia_jle_Jd    },
  /* 0F 8F */ { 0, &Ia_jnle_Jd   },
  /* 0F 90 */ { 0, &Ia_seto_Eb   },
  /* 0F 91 */ { 0, &Ia_setno_Eb  },
  /* 0F 92 */ { 0, &Ia_setb_Eb   },
  /* 0F 93 */ { 0, &Ia_setnb_Eb  },
  /* 0F 94 */ { 0, &Ia_setz_Eb   },
  /* 0F 95 */ { 0, &Ia_setnz_Eb  },
  /* 0F 96 */ { 0, &Ia_setbe_Eb  },
  /* 0F 97 */ { 0, &Ia_setnbe_Eb },
  /* 0F 98 */ { 0, &Ia_sets_Eb   },
  /* 0F 99 */ { 0, &Ia_setns_Eb  },
  /* 0F 9A */ { 0, &Ia_setp_Eb   },
  /* 0F 9B */ { 0, &Ia_setnp_Eb  },
  /* 0F 9C */ { 0, &Ia_setl_Eb   },
  /* 0F 9D */ { 0, &Ia_setnl_Eb  },
  /* 0F 9E */ { 0, &Ia_setle_Eb  },
  /* 0F 9F */ { 0, &Ia_setnle_Eb },
  /* 0F A0 */ { 0, &Ia_pushl_FS  },
  /* 0F A1 */ { 0, &Ia_popl_FS   },
  /* 0F A2 */ { 0, &Ia_cpuid     },
  /* 0F A3 */ { 0, &Ia_btl_Ed_Gd },
  /* 0F A4 */ { 0, &Ia_shldl_Ed_Gd_Ib },
  /* 0F A5 */ { 0, &Ia_shldl_Ed_Gd_CL },
  /* 0F A6 */ { 0, &Ia_Invalid    },
  /* 0F A7 */ { 0, &Ia_Invalid    },
  /* 0F A8 */ { 0, &Ia_pushl_GS   },
  /* 0F A9 */ { 0, &Ia_popl_GS    },
  /* 0F AA */ { 0, &Ia_rsm        },
  /* 0F AB */ { 0, &Ia_btsl_Ed_Gd },
  /* 0F AC */ { 0, &Ia_shrdl_Ed_Gd_Ib },
  /* 0F AD */ { 0, &Ia_shrdl_Ed_Gd_CL },
  /* 0F AE */ { GRPN(G15)         },
  /* 0F AF */ { 0, &Ia_imull_Gd_Ed    },
  /* 0F B0 */ { 0, &Ia_cmpxchgb_Eb_Gb },
  /* 0F B1 */ { 0, &Ia_cmpxchgl_Ed_Gd },
  /* 0F B2 */ { 0, &Ia_lssl_Gd_Mp   },
  /* 0F B3 */ { 0, &Ia_btrl_Ed_Gd   },
  /* 0F B4 */ { 0, &Ia_lfsl_Gd_Mp   },
  /* 0F B5 */ { 0, &Ia_lgsl_Gd_Mp   },
  /* 0F B6 */ { 0, &Ia_movzbl_Gd_Eb },
  /* 0F B7 */ { 0, &Ia_movzwl_Gd_Ew },
  /* 0F B8 */ { 0, &Ia_Invalid      },
  /* 0F B9 */ { 0, &Ia_ud2b         },
  /* 0F BA */ { GRPN(G8EdIb)        },
  /* 0F BB */ { 0, &Ia_btcl_Ed_Gd   },
  /* 0F BC */ { 0, &Ia_bsfl_Gd_Ed   },
  /* 0F BD */ { 0, &Ia_bsrl_Gd_Ed   },
  /* 0F BE */ { 0, &Ia_movsbl_Gd_Eb },
  /* 0F BF */ { 0, &Ia_movswl_Gd_Ew },
  /* 0F C0 */ { 0, &Ia_xaddb_Eb_Gb  },
  /* 0F C0 */ { 0, &Ia_xaddl_Ed_Gd  },
  /* 0F C2 */ { GRPSSE(0fc2)      },
  /* 0F C3 */ { GRPSSE(0fc3)      },
  /* 0F C4 */ { GRPSSE(0fc4)      },
  /* 0F C5 */ { GRPSSE(0fc5)      },
  /* 0F C6 */ { GRPSSE(0fc6)      },
  /* 0F C7 */ { GRPN(G9)          },
  /* 0F C8 */ { 0, &Ia_bswapl_ERX },
  /* 0F C9 */ { 0, &Ia_bswapl_ERX },
  /* 0F CA */ { 0, &Ia_bswapl_ERX },
  /* 0F CB */ { 0, &Ia_bswapl_ERX },
  /* 0F CC */ { 0, &Ia_bswapl_ERX },
  /* 0F CD */ { 0, &Ia_bswapl_ERX },
  /* 0F CE */ { 0, &Ia_bswapl_ERX },
  /* 0F CF */ { 0, &Ia_bswapl_ERX },
  /* 0F D0 */ { GRPSSE(0fd0) },
  /* 0F D1 */ { GRPSSE(0fd1) },
  /* 0F D2 */ { GRPSSE(0fd2) },
  /* 0F D3 */ { GRPSSE(0fd3) },
  /* 0F D4 */ { GRPSSE(0fd4) },
  /* 0F D5 */ { GRPSSE(0fd5) },
  /* 0F D6 */ { GRPSSE(0fd6) },
  /* 0F D7 */ { GRPSSE(0fd7) },
  /* 0F D8 */ { GRPSSE(0fd8) },
  /* 0F D9 */ { GRPSSE(0fd9) },
  /* 0F DA */ { GRPSSE(0fda) },
  /* 0F DB */ { GRPSSE(0fdb) },
  /* 0F DC */ { GRPSSE(0fdc) },
  /* 0F DD */ { GRPSSE(0fdd) },
  /* 0F DE */ { GRPSSE(0fde) },
  /* 0F DF */ { GRPSSE(0fdf) },
  /* 0F E0 */ { GRPSSE(0fe0) },
  /* 0F E1 */ { GRPSSE(0fe1) },
  /* 0F E2 */ { GRPSSE(0fe2) },
  /* 0F E3 */ { GRPSSE(0fe3) },
  /* 0F E4 */ { GRPSSE(0fe4) },
  /* 0F E5 */ { GRPSSE(0fe5) },
  /* 0F E6 */ { GRPSSE(0fe6) },
  /* 0F E7 */ { GRPSSE(0fe7) },
  /* 0F E8 */ { GRPSSE(0fe8) },
  /* 0F E9 */ { GRPSSE(0fe9) },
  /* 0F EA */ { GRPSSE(0fea) },
  /* 0F EB */ { GRPSSE(0feb) },
  /* 0F EC */ { GRPSSE(0fec) },
  /* 0F ED */ { GRPSSE(0fed) },
  /* 0F EE */ { GRPSSE(0fee) },
  /* 0F EF */ { GRPSSE(0fef) },
  /* 0F F0 */ { GRPSSE(0ff0) },
  /* 0F F1 */ { GRPSSE(0ff1) },
  /* 0F F2 */ { GRPSSE(0ff2) },
  /* 0F F3 */ { GRPSSE(0ff3) },
  /* 0F F4 */ { GRPSSE(0ff4) },
  /* 0F F5 */ { GRPSSE(0ff5) },
  /* 0F F6 */ { GRPSSE(0ff6) },
  /* 0F F7 */ { GRPSSE(0ff7) },
  /* 0F F8 */ { GRPSSE(0ff8) },
  /* 0F F9 */ { GRPSSE(0ff9) },
  /* 0F FA */ { GRPSSE(0ffa) },
  /* 0F FB */ { GRPSSE(0ffb) },
  /* 0F FC */ { GRPSSE(0ffc) },
  /* 0F FD */ { GRPSSE(0ffd) },
  /* 0F FE */ { GRPSSE(0ffe) },
  /* 0F FF */ { 0, &Ia_Invalid }
};

/* ************************************************************************ */
/* Long mode */

static BxDisasmOpcodeTable_t BxDisasmOpcodes64w[256*2] = {
  // 256 entries for single byte opcodes
  /* 00 */ { 0, &Ia_addb_Eb_Gb },
  /* 01 */ { 0, &Ia_addw_Ew_Gw },
  /* 02 */ { 0, &Ia_addb_Gb_Eb },
  /* 03 */ { 0, &Ia_addw_Gw_Ew },
  /* 04 */ { 0, &Ia_addb_AL_Ib },
  /* 05 */ { 0, &Ia_addw_AX_Iw },
  /* 06 */ { 0, &Ia_Invalid    },
  /* 07 */ { 0, &Ia_Invalid    },
  /* 08 */ { 0, &Ia_orb_Eb_Gb  },
  /* 09 */ { 0, &Ia_orw_Ew_Gw  },
  /* 0A */ { 0, &Ia_orb_Gb_Eb  },
  /* 0B */ { 0, &Ia_orw_Gw_Ew  },
  /* 0C */ { 0, &Ia_orb_AL_Ib  },
  /* 0D */ { 0, &Ia_orw_AX_Iw  },
  /* 0E */ { 0, &Ia_Invalid    },
  /* 0F */ { 0, &Ia_error      },   // 2 byte escape
  /* 10 */ { 0, &Ia_adcb_Eb_Gb },
  /* 11 */ { 0, &Ia_adcw_Ew_Gw },
  /* 12 */ { 0, &Ia_adcb_Gb_Eb },
  /* 13 */ { 0, &Ia_adcw_Gw_Ew },
  /* 14 */ { 0, &Ia_adcb_AL_Ib },
  /* 15 */ { 0, &Ia_adcw_AX_Iw },
  /* 16 */ { 0, &Ia_Invalid    },
  /* 17 */ { 0, &Ia_Invalid    },
  /* 18 */ { 0, &Ia_sbbb_Eb_Gb },
  /* 19 */ { 0, &Ia_sbbw_Ew_Gw },
  /* 1A */ { 0, &Ia_sbbb_Gb_Eb },
  /* 1B */ { 0, &Ia_sbbw_Gw_Ew },
  /* 1C */ { 0, &Ia_sbbb_AL_Ib },
  /* 1D */ { 0, &Ia_sbbw_AX_Iw },
  /* 1E */ { 0, &Ia_Invalid    },
  /* 1F */ { 0, &Ia_Invalid    },
  /* 20 */ { 0, &Ia_andb_Eb_Gb },
  /* 21 */ { 0, &Ia_andw_Ew_Gw },
  /* 22 */ { 0, &Ia_andb_Gb_Eb },
  /* 23 */ { 0, &Ia_andw_Gw_Ew },
  /* 24 */ { 0, &Ia_andb_AL_Ib },
  /* 25 */ { 0, &Ia_andw_AX_Iw },
  /* 26 */ { 0, &Ia_prefix_es  },   // ES:
  /* 27 */ { 0, &Ia_Invalid    },
  /* 28 */ { 0, &Ia_subb_Eb_Gb },
  /* 29 */ { 0, &Ia_subw_Ew_Gw },
  /* 2A */ { 0, &Ia_subb_Gb_Eb },
  /* 2B */ { 0, &Ia_subw_Gw_Ew },
  /* 2C */ { 0, &Ia_subb_AL_Ib },
  /* 2D */ { 0, &Ia_subw_AX_Iw },
  /* 2E */ { 0, &Ia_prefix_cs  },   // CS:
  /* 2F */ { 0, &Ia_Invalid    },
  /* 30 */ { 0, &Ia_xorb_Eb_Gb },
  /* 31 */ { 0, &Ia_xorw_Ew_Gw },
  /* 32 */ { 0, &Ia_xorb_Gb_Eb },
  /* 33 */ { 0, &Ia_xorw_Gw_Ew },
  /* 34 */ { 0, &Ia_xorb_AL_Ib },
  /* 35 */ { 0, &Ia_xorw_AX_Iw },
  /* 36 */ { 0, &Ia_prefix_ss  },   // SS:
  /* 37 */ { 0, &Ia_Invalid    },
  /* 38 */ { 0, &Ia_cmpb_Eb_Gb },
  /* 39 */ { 0, &Ia_cmpw_Ew_Gw },
  /* 3A */ { 0, &Ia_cmpb_Gb_Eb },
  /* 3B */ { 0, &Ia_cmpw_Gw_Ew },
  /* 3C */ { 0, &Ia_cmpb_AL_Ib },
  /* 3D */ { 0, &Ia_cmpw_AX_Iw },
  /* 3E */ { 0, &Ia_prefix_ds  },   // DS:
  /* 3F */ { 0, &Ia_Invalid    },
  /* 40 */ { 0, &Ia_prefix_rex },   // REX:
  /* 41 */ { 0, &Ia_prefix_rex },   // REX:
  /* 42 */ { 0, &Ia_prefix_rex },   // REX:
  /* 43 */ { 0, &Ia_prefix_rex },   // REX:
  /* 44 */ { 0, &Ia_prefix_rex },   // REX:
  /* 45 */ { 0, &Ia_prefix_rex },   // REX:
  /* 46 */ { 0, &Ia_prefix_rex },   // REX:
  /* 47 */ { 0, &Ia_prefix_rex },   // REX:
  /* 48 */ { 0, &Ia_prefix_rex },   // REX:
  /* 49 */ { 0, &Ia_prefix_rex },   // REX:
  /* 4A */ { 0, &Ia_prefix_rex },   // REX:
  /* 4B */ { 0, &Ia_prefix_rex },   // REX:
  /* 4C */ { 0, &Ia_prefix_rex },   // REX:
  /* 4D */ { 0, &Ia_prefix_rex },   // REX:
  /* 4E */ { 0, &Ia_prefix_rex },   // REX:
  /* 4F */ { 0, &Ia_prefix_rex },   // REX:
  /* 50 */ { 0, &Ia_pushw_RX   },
  /* 51 */ { 0, &Ia_pushw_RX   },
  /* 52 */ { 0, &Ia_pushw_RX   },
  /* 53 */ { 0, &Ia_pushw_RX   },
  /* 54 */ { 0, &Ia_pushw_RX   },
  /* 55 */ { 0, &Ia_pushw_RX   },
  /* 56 */ { 0, &Ia_pushw_RX   },
  /* 57 */ { 0, &Ia_pushw_RX   },
  /* 58 */ { 0, &Ia_popw_RX    },
  /* 59 */ { 0, &Ia_popw_RX    },
  /* 5A */ { 0, &Ia_popw_RX    },
  /* 5B */ { 0, &Ia_popw_RX    },
  /* 5C */ { 0, &Ia_popw_RX    },
  /* 5D */ { 0, &Ia_popw_RX    },
  /* 5E */ { 0, &Ia_popw_RX    },
  /* 5F */ { 0, &Ia_popw_RX    },
  /* 60 */ { 0, &Ia_Invalid    },
  /* 61 */ { 0, &Ia_Invalid    },
  /* 62 */ { 0, &Ia_Invalid    },
  /* 63 */ { 0, &Ia_movw_Gw_Ew },
  /* 64 */ { 0, &Ia_prefix_fs  },   // FS:
  /* 65 */ { 0, &Ia_prefix_gs  },   // GS:
  /* 66 */ { 0, &Ia_prefix_osize }, // OSIZE:
  /* 67 */ { 0, &Ia_prefix_asize }, // ASIZE:
  /* 68 */ { 0, &Ia_pushw_Iw    },
  /* 69 */ { 0, &Ia_imulw_Gw_Ew_Iw  },
  /* 6A */ { 0, &Ia_pushw_sIb   },
  /* 6B */ { 0, &Ia_imulw_Gw_Ew_sIb },
  /* 6C */ { 0, &Ia_insb_Yb_DX  },
  /* 6D */ { 0, &Ia_insw_Yw_DX  },
  /* 6E */ { 0, &Ia_outsb_DX_Xb },
  /* 6F */ { 0, &Ia_outsw_DX_Xw },
  /* 70 */ { 0, &Ia_jo_Jb       },
  /* 71 */ { 0, &Ia_jno_Jb      },
  /* 72 */ { 0, &Ia_jb_Jb       },
  /* 73 */ { 0, &Ia_jnb_Jb      },
  /* 74 */ { 0, &Ia_jz_Jb       },
  /* 75 */ { 0, &Ia_jnz_Jb      },
  /* 76 */ { 0, &Ia_jbe_Jb      },
  /* 77 */ { 0, &Ia_jnbe_Jb     },
  /* 78 */ { 0, &Ia_js_Jb       },
  /* 79 */ { 0, &Ia_jns_Jb      },
  /* 7A */ { 0, &Ia_jp_Jb       },
  /* 7B */ { 0, &Ia_jnp_Jb      },
  /* 7C */ { 0, &Ia_jl_Jb       },
  /* 7D */ { 0, &Ia_jnl_Jb      },
  /* 7E */ { 0, &Ia_jle_Jb      },
  /* 7F */ { 0, &Ia_jnle_Jb     },
  /* 80 */ { GRPN(G1EbIb)       },
  /* 81 */ { GRPN(G1EwIw)       },
  /* 82 */ { 9, &Ia_Invalid     },
  /* 83 */ { GRPN(G1EwIb)       },
  /* 84 */ { 0, &Ia_testb_Eb_Gb },
  /* 85 */ { 0, &Ia_testw_Ew_Gw },
  /* 86 */ { 0, &Ia_xchgb_Eb_Gb },
  /* 87 */ { 0, &Ia_xchgw_Ew_Gw },
  /* 88 */ { 0, &Ia_movb_Eb_Gb  },
  /* 89 */ { 0, &Ia_movw_Ew_Gw  },
  /* 8A */ { 0, &Ia_movb_Gb_Eb  },
  /* 8B */ { 0, &Ia_movw_Gw_Ew  },
  /* 8C */ { 0, &Ia_movw_Ew_Sw  },
  /* 8D */ { 0, &Ia_leaw_Gw_Mw  },
  /* 8E */ { 0, &Ia_movw_Sw_Ew  },
  /* 8F */ { 0, &Ia_popw_Ew     },
  /* 90 */ { 0, &Ia_xchgw_RX_AX }, // handle XCHG R8w, AX
  /* 91 */ { 0, &Ia_xchgw_RX_AX },
  /* 92 */ { 0, &Ia_xchgw_RX_AX },
  /* 93 */ { 0, &Ia_xchgw_RX_AX },
  /* 94 */ { 0, &Ia_xchgw_RX_AX },
  /* 95 */ { 0, &Ia_xchgw_RX_AX },
  /* 96 */ { 0, &Ia_xchgw_RX_AX },
  /* 97 */ { 0, &Ia_xchgw_RX_AX },
  /* 98 */ { 0, &Ia_cbw         },
  /* 99 */ { 0, &Ia_cwd         },
  /* 9A */ { 0, &Ia_Invalid     },
  /* 9B */ { 0, &Ia_fwait       },
  /* 9C */ { 0, &Ia_pushfw      },
  /* 9D */ { 0, &Ia_popfw       },
  /* 9E */ { 0, &Ia_sahf        },
  /* 9F */ { 0, &Ia_lahf        },
  /* A0 */ { 0, &Ia_movb_AL_Ob  },
  /* A1 */ { 0, &Ia_movw_AX_Ow  },
  /* A0 */ { 0, &Ia_movb_Ob_AL  },
  /* A1 */ { 0, &Ia_movw_Ow_AX  },
  /* A4 */ { 0, &Ia_movsb_Yb_Xb },
  /* A5 */ { 0, &Ia_movsw_Yw_Xw },
  /* A6 */ { 0, &Ia_cmpsb_Yb_Xb },
  /* A7 */ { 0, &Ia_cmpsw_Yw_Xw },
  /* A8 */ { 0, &Ia_testb_AL_Ib },
  /* A9 */ { 0, &Ia_testw_AX_Iw },
  /* AA */ { 0, &Ia_stosb_Yb_AL },
  /* AB */ { 0, &Ia_stosw_Yw_AX },
  /* AC */ { 0, &Ia_lodsb_AL_Xb },
  /* AD */ { 0, &Ia_lodsw_AX_Xw },
  /* AE */ { 0, &Ia_scasb_Yb_AL },
  /* AF */ { 0, &Ia_scasw_Yw_AX },
  /* B0 */ { 0, &Ia_movb_R8_Ib  },
  /* B1 */ { 0, &Ia_movb_R8_Ib  },
  /* B2 */ { 0, &Ia_movb_R8_Ib  },
  /* B3 */ { 0, &Ia_movb_R8_Ib  },
  /* B4 */ { 0, &Ia_movb_R8_Ib  },
  /* B5 */ { 0, &Ia_movb_R8_Ib  },
  /* B6 */ { 0, &Ia_movb_R8_Ib  },
  /* B7 */ { 0, &Ia_movb_R8_Ib  },
  /* B8 */ { 0, &Ia_movw_RX_Iw  },
  /* B9 */ { 0, &Ia_movw_RX_Iw  },
  /* BA */ { 0, &Ia_movw_RX_Iw  },
  /* BB */ { 0, &Ia_movw_RX_Iw  },
  /* BC */ { 0, &Ia_movw_RX_Iw  },
  /* BD */ { 0, &Ia_movw_RX_Iw  },
  /* BE */ { 0, &Ia_movw_RX_Iw  },
  /* BF */ { 0, &Ia_movw_RX_Iw  },
  /* C0 */ { GRPN(G2Eb)         },
  /* C1 */ { GRPN(G2Ew)         },
  /* C2 */ { 0, &Ia_ret_Iw      },
  /* C3 */ { 0, &Ia_ret         },
  /* C4 */ { 0, &Ia_Invalid     },
  /* C5 */ { 0, &Ia_Invalid     },
  /* C6 */ { 0, &Ia_movb_Eb_Ib  },
  /* C7 */ { 0, &Ia_movw_Ew_Iw  },
  /* C8 */ { 0, &Ia_enter       },
  /* C9 */ { 0, &Ia_leave       },
  /* CA */ { 0, &Ia_lret_Iw     },
  /* CB */ { 0, &Ia_lret        },
  /* CC */ { 0, &Ia_int3        },
  /* CD */ { 0, &Ia_int_Ib      },
  /* CE */ { 0, &Ia_Invalid     },
  /* CF */ { 0, &Ia_iretw       },
  /* D0 */ { GRPN(G2EbI1)       },
  /* D1 */ { GRPN(G2EwI1)       },
  /* D2 */ { GRPN(G2EbCL)       },
  /* D3 */ { GRPN(G2EwCL)       },
  /* D4 */ { 0, &Ia_Invalid     },
  /* D5 */ { 0, &Ia_Invalid     },
  /* D6 */ { 0, &Ia_Invalid     },
  /* D7 */ { 0, &Ia_xlat        },
  /* D8 */ { GRPFP(D8)          },
  /* D9 */ { GRPFP(D9)          },
  /* DA */ { GRPFP(DA)          },
  /* DB */ { GRPFP(DB)          },
  /* DC */ { GRPFP(DC)          },
  /* DD */ { GRPFP(DD)          },
  /* DE */ { GRPFP(DE)          },
  /* DF */ { GRPFP(DF)          },
  /* E0 */ { 0, &Ia_loopne_Jb   },
  /* E1 */ { 0, &Ia_loope_Jb    },
  /* E2 */ { 0, &Ia_loop_Jb     },
  /* E3 */ { 0, &Ia_jrcxz_Jb    },
  /* E4 */ { 0, &Ia_inb_AL_Ib   },
  /* E5 */ { 0, &Ia_inw_AX_Ib   },
  /* E6 */ { 0, &Ia_outb_Ib_AL  },
  /* E7 */ { 0, &Ia_outw_Ib_AX  },
  /* E8 */ { 0, &Ia_call_Jd     },
  /* E9 */ { 0, &Ia_jmp_Jd      },
  /* EA */ { 0, &Ia_Invalid     },
  /* EB */ { 0, &Ia_jmp_Jb      },
  /* EC */ { 0, &Ia_inb_AL_DX   },
  /* ED */ { 0, &Ia_inw_AX_DX   },
  /* EE */ { 0, &Ia_outb_DX_AL  },
  /* EF */ { 0, &Ia_outw_DX_AX  },
  /* F0 */ { 0, &Ia_prefix_lock },   // LOCK:
  /* F1 */ { 0, &Ia_int1        },
  /* F2 */ { 0, &Ia_prefix_repne },  // REPNE:
  /* F3 */ { 0, &Ia_prefix_rep  },   // REP:
  /* F4 */ { 0, &Ia_hlt         },
  /* F5 */ { 0, &Ia_cmc         },
  /* F6 */ { GRPN(G3Eb)         },
  /* F7 */ { GRPN(G3Ew)         },
  /* F8 */ { 0, &Ia_clc         },
  /* F9 */ { 0, &Ia_stc         },
  /* FA */ { 0, &Ia_cli         },
  /* FB */ { 0, &Ia_sti         },
  /* FC */ { 0, &Ia_cld         },
  /* FD */ { 0, &Ia_std         },
  /* FE */ { GRPN(G4)           },
  /* FF */ { GRPN(G5w)          },

  // 256 entries for two byte opcodes
  /* 0F 00 */ { GRPN(G6)          },
  /* 0F 01 */ { GRPN(G7)          },
  /* 0F 02 */ { 0, &Ia_larw_Gw_Ew },
  /* 0F 03 */ { 0, &Ia_lslw_Gw_Ew },
  /* 0F 04 */ { 0, &Ia_Invalid    },
  /* 0F 05 */ { 0, &Ia_syscall    },
  /* 0F 06 */ { 0, &Ia_clts       },
  /* 0F 07 */ { 0, &Ia_sysret     },
  /* 0F 08 */ { 0, &Ia_invd       },
  /* 0F 09 */ { 0, &Ia_wbinvd     },
  /* 0F 0A */ { 0, &Ia_Invalid    },
  /* 0F 0B */ { 0, &Ia_ud2a       },
  /* 0F 0C */ { 0, &Ia_Invalid    },
  /* 0F 0D */ { 0, &Ia_prefetch   },   // 3DNow!
  /* 0F 0E */ { 0, &Ia_femms      },   // 3DNow!
  /* 0F 0F */ { GRP3DNOW          },
  /* 0F 10 */ { GRPSSE(0f10)      },
  /* 0F 11 */ { GRPSSE(0f11)      },
  /* 0F 12 */ { GRPSSE(0f12)      },
  /* 0F 13 */ { GRPSSE(0f13)      },
  /* 0F 14 */ { GRPSSE(0f14)      },
  /* 0F 15 */ { GRPSSE(0f15)      },
  /* 0F 16 */ { GRPSSE(0f16)      },
  /* 0F 17 */ { GRPSSE(0f17)      },
  /* 0F 18 */ { GRPN(G16)         },
  /* 0F 19 */ { 0, &Ia_Invalid    },
  /* 0F 1A */ { 0, &Ia_Invalid    },
  /* 0F 1B */ { 0, &Ia_Invalid    },
  /* 0F 1C */ { 0, &Ia_Invalid    },
  /* 0F 1D */ { 0, &Ia_Invalid    },
  /* 0F 1E */ { 0, &Ia_Invalid    },
  /* 0F 1F */ { 0, &Ia_multibyte_nop },
  /* 0F 20 */ { 0, &Ia_movq_Rq_Cq },
  /* 0F 21 */ { 0, &Ia_movq_Rq_Dq },
  /* 0F 22 */ { 0, &Ia_movq_Cq_Rq },
  /* 0F 23 */ { 0, &Ia_movq_Dq_Rq },
  /* 0F 24 */ { 0, &Ia_Invalid    },
  /* 0F 25 */ { 0, &Ia_Invalid    },
  /* 0F 26 */ { 0, &Ia_Invalid    },
  /* 0F 27 */ { 0, &Ia_Invalid    },
  /* 0F 28 */ { GRPSSE(0f28) },
  /* 0F 29 */ { GRPSSE(0f29) },
  /* 0F 2A */ { GRPSSE(0f2a) },
  /* 0F 2B */ { GRPSSE(0f2b) },
  /* 0F 2C */ { GRPSSE(0f2c) },
  /* 0F 2D */ { GRPSSE(0f2d) },
  /* 0F 2E */ { GRPSSE(0f2e) },
  /* 0F 2F */ { GRPSSE(0f2f) },
  /* 0F 30 */ { 0, &Ia_wrmsr },
  /* 0F 31 */ { 0, &Ia_rdtsc },
  /* 0F 32 */ { 0, &Ia_rdmsr },
  /* 0F 33 */ { 0, &Ia_rdpmc },
  /* 0F 34 */ { 0, &Ia_sysenter },
  /* 0F 35 */ { 0, &Ia_sysexit  },
  /* 0F 36 */ { 0, &Ia_Invalid  },
  /* 0F 37 */ { 0, &Ia_Invalid  },
  /* 0F 38 */ { GR3BTAB(A4) },
  /* 0F 39 */ { 0, &Ia_Invalid  },
  /* 0F 3A */ { GR3BTAB(A5) },
  /* 0F 3B */ { 0, &Ia_Invalid  },
  /* 0F 3C */ { 0, &Ia_Invalid  },
  /* 0F 3D */ { 0, &Ia_Invalid  },
  /* 0F 3E */ { 0, &Ia_Invalid  },
  /* 0F 3F */ { 0, &Ia_Invalid  },
  /* 0F 40 */ { 0, &Ia_cmovow_Gw_Ew  },
  /* 0F 41 */ { 0, &Ia_cmovnow_Gw_Ew },
  /* 0F 42 */ { 0, &Ia_cmovcw_Gw_Ew  },
  /* 0F 43 */ { 0, &Ia_cmovncw_Gw_Ew },
  /* 0F 44 */ { 0, &Ia_cmovzw_Gw_Ew  },
  /* 0F 45 */ { 0, &Ia_cmovnzw_Gw_Ew },
  /* 0F 46 */ { 0, &Ia_cmovnaw_Gw_Ew },
  /* 0F 47 */ { 0, &Ia_cmovaw_Gw_Ew  },
  /* 0F 48 */ { 0, &Ia_cmovsw_Gw_Ew  },
  /* 0F 49 */ { 0, &Ia_cmovnsw_Gw_Ew },
  /* 0F 4A */ { 0, &Ia_cmovpw_Gw_Ew  },
  /* 0F 4B */ { 0, &Ia_cmovnpw_Gw_Ew },
  /* 0F 4C */ { 0, &Ia_cmovlw_Gw_Ew  },
  /* 0F 4D */ { 0, &Ia_cmovnlw_Gw_Ew },
  /* 0F 4E */ { 0, &Ia_cmovngw_Gw_Ew },
  /* 0F 4F */ { 0, &Ia_cmovgw_Gw_Ew  },
  /* 0F 50 */ { GRPSSE(0f50) },
  /* 0F 51 */ { GRPSSE(0f51) },
  /* 0F 52 */ { GRPSSE(0f52) },
  /* 0F 53 */ { GRPSSE(0f53) },
  /* 0F 54 */ { GRPSSE(0f54) },
  /* 0F 55 */ { GRPSSE(0f55) },
  /* 0F 56 */ { GRPSSE(0f56) },
  /* 0F 57 */ { GRPSSE(0f57) },
  /* 0F 58 */ { GRPSSE(0f58) },
  /* 0F 59 */ { GRPSSE(0f59) },
  /* 0F 5A */ { GRPSSE(0f5a) },
  /* 0F 5B */ { GRPSSE(0f5b) },
  /* 0F 5C */ { GRPSSE(0f5c) },
  /* 0F 5D */ { GRPSSE(0f5d) },
  /* 0F 5E */ { GRPSSE(0f5e) },
  /* 0F 5F */ { GRPSSE(0f5f) },
  /* 0F 60 */ { GRPSSE(0f60) },
  /* 0F 61 */ { GRPSSE(0f61) },
  /* 0F 62 */ { GRPSSE(0f62) },
  /* 0F 63 */ { GRPSSE(0f63) },
  /* 0F 64 */ { GRPSSE(0f64) },
  /* 0F 65 */ { GRPSSE(0f65) },
  /* 0F 66 */ { GRPSSE(0f66) },
  /* 0F 67 */ { GRPSSE(0f67) },
  /* 0F 68 */ { GRPSSE(0f68) },
  /* 0F 69 */ { GRPSSE(0f69) },
  /* 0F 6A */ { GRPSSE(0f6a) },
  /* 0F 6B */ { GRPSSE(0f6b) },
  /* 0F 6C */ { GRPSSE(0f6c) },
  /* 0F 6D */ { GRPSSE(0f6d) },
  /* 0F 6E */ { GRPSSE(0f6e) },
  /* 0F 6F */ { GRPSSE(0f6f) },
  /* 0F 70 */ { GRPSSE(0f70) },
  /* 0F 71 */ { GRPN(G12)    },
  /* 0F 72 */ { GRPN(G13)    },
  /* 0F 73 */ { GRPN(G14)    },
  /* 0F 74 */ { GRPSSE(0f74) },
  /* 0F 75 */ { GRPSSE(0f75) },
  /* 0F 76 */ { GRPSSE(0f76) },
  /* 0F 77 */ { 0, &Ia_emms  },
  /* 0F 78 */ { 0, &Ia_Invalid },
  /* 0F 79 */ { 0, &Ia_Invalid },
  /* 0F 7A */ { 0, &Ia_Invalid },
  /* 0F 7B */ { 0, &Ia_Invalid },
  /* 0F 7C */ { GRPSSE(0f7c) },
  /* 0F 7D */ { GRPSSE(0f7d) },
  /* 0F 7E */ { GRPSSE(0f7e) },
  /* 0F 7F */ { GRPSSE(0f7f) },
  /* 0F 80 */ { 0, &Ia_jo_Jd     },
  /* 0F 81 */ { 0, &Ia_jno_Jd    },
  /* 0F 82 */ { 0, &Ia_jb_Jd     },
  /* 0F 83 */ { 0, &Ia_jnb_Jd    },
  /* 0F 84 */ { 0, &Ia_jz_Jd     },
  /* 0F 85 */ { 0, &Ia_jnz_Jd    },
  /* 0F 86 */ { 0, &Ia_jbe_Jd    },
  /* 0F 87 */ { 0, &Ia_jnbe_Jd   },
  /* 0F 88 */ { 0, &Ia_js_Jd     },
  /* 0F 89 */ { 0, &Ia_jns_Jd    },
  /* 0F 8A */ { 0, &Ia_jp_Jd     },
  /* 0F 8B */ { 0, &Ia_jnp_Jd    },
  /* 0F 8C */ { 0, &Ia_jl_Jd     },
  /* 0F 8D */ { 0, &Ia_jnl_Jd    },
  /* 0F 8E */ { 0, &Ia_jle_Jd    },
  /* 0F 8F */ { 0, &Ia_jnle_Jd   },
  /* 0F 90 */ { 0, &Ia_seto_Eb   },
  /* 0F 91 */ { 0, &Ia_setno_Eb  },
  /* 0F 92 */ { 0, &Ia_setb_Eb   },
  /* 0F 93 */ { 0, &Ia_setnb_Eb  },
  /* 0F 94 */ { 0, &Ia_setz_Eb   },
  /* 0F 95 */ { 0, &Ia_setnz_Eb  },
  /* 0F 96 */ { 0, &Ia_setbe_Eb  },
  /* 0F 97 */ { 0, &Ia_setnbe_Eb },
  /* 0F 98 */ { 0, &Ia_sets_Eb   },
  /* 0F 99 */ { 0, &Ia_setns_Eb  },
  /* 0F 9A */ { 0, &Ia_setp_Eb   },
  /* 0F 9B */ { 0, &Ia_setnp_Eb  },
  /* 0F 9C */ { 0, &Ia_setl_Eb   },
  /* 0F 9D */ { 0, &Ia_setnl_Eb  },
  /* 0F 9E */ { 0, &Ia_setle_Eb  },
  /* 0F 9F */ { 0, &Ia_setnle_Eb },
  /* 0F A0 */ { 0, &Ia_pushw_FS  },
  /* 0F A1 */ { 0, &Ia_popw_FS   },
  /* 0F A2 */ { 0, &Ia_cpuid     },
  /* 0F A3 */ { 0, &Ia_btw_Ew_Gw },
  /* 0F A4 */ { 0, &Ia_shldw_Ew_Gw_Ib },
  /* 0F A5 */ { 0, &Ia_shldw_Ew_Gw_CL },
  /* 0F A6 */ { 0, &Ia_Invalid        },
  /* 0F A7 */ { 0, &Ia_Invalid        },
  /* 0F A8 */ { 0, &Ia_pushw_GS       },
  /* 0F A9 */ { 0, &Ia_popw_GS        },
  /* 0F AA */ { 0, &Ia_rsm            },
  /* 0F AB */ { 0, &Ia_btsw_Ew_Gw     },
  /* 0F AC */ { 0, &Ia_shrdw_Ew_Gw_Ib },
  /* 0F AD */ { 0, &Ia_shrdw_Ew_Gw_CL },
  /* 0F AE */ { GRPN(G15)             },
  /* 0F AF */ { 0, &Ia_imulw_Gw_Ew    },
  /* 0F B0 */ { 0, &Ia_cmpxchgb_Eb_Gb },
  /* 0F B1 */ { 0, &Ia_cmpxchgw_Ew_Gw },
  /* 0F B2 */ { 0, &Ia_lssw_Gw_Mp     },
  /* 0F B3 */ { 0, &Ia_btrw_Ew_Gw     },
  /* 0F B4 */ { 0, &Ia_lfsw_Gw_Mp     },
  /* 0F B5 */ { 0, &Ia_lgsw_Gw_Mp     },
  /* 0F B6 */ { 0, &Ia_movzbw_Gw_Eb   },
  /* 0F B7 */ { 0, &Ia_movw_Gw_Ew     },
  /* 0F B8 */ { 0, &Ia_Invalid        },
  /* 0F B9 */ { 0, &Ia_ud2b           },
  /* 0F BA */ { GRPN(G8EwIb)          },
  /* 0F BB */ { 0, &Ia_btcw_Ew_Gw     },
  /* 0F BC */ { 0, &Ia_bsfw_Gw_Ew     },
  /* 0F BD */ { 0, &Ia_bsrw_Gw_Ew     },
  /* 0F BE */ { 0, &Ia_movsbw_Gw_Eb   },
  /* 0F BF */ { 0, &Ia_movw_Gw_Ew     },
  /* 0F C0 */ { 0, &Ia_xaddb_Eb_Gb    },
  /* 0F C0 */ { 0, &Ia_xaddw_Ew_Gw    },
  /* 0F C2 */ { GRPSSE(0fc2) },
  /* 0F C3 */ { GRPSSE(0fc3) },
  /* 0F C4 */ { GRPSSE(0fc4) },
  /* 0F C5 */ { GRPSSE(0fc5) },
  /* 0F C6 */ { GRPSSE(0fc6) },
  /* 0F C7 */ { GRPN(G9)     },
  /* 0F C8 */ { 0, &Ia_bswapl_ERX },
  /* 0F C9 */ { 0, &Ia_bswapl_ERX },
  /* 0F CA */ { 0, &Ia_bswapl_ERX },
  /* 0F CB */ { 0, &Ia_bswapl_ERX },
  /* 0F CC */ { 0, &Ia_bswapl_ERX },
  /* 0F CD */ { 0, &Ia_bswapl_ERX },
  /* 0F CE */ { 0, &Ia_bswapl_ERX },
  /* 0F CF */ { 0, &Ia_bswapl_ERX },
  /* 0F D0 */ { GRPSSE(0fd0) },
  /* 0F D1 */ { GRPSSE(0fd1) },
  /* 0F D2 */ { GRPSSE(0fd2) },
  /* 0F D3 */ { GRPSSE(0fd3) },
  /* 0F D4 */ { GRPSSE(0fd4) },
  /* 0F D5 */ { GRPSSE(0fd5) },
  /* 0F D6 */ { GRPSSE(0fd6) },
  /* 0F D7 */ { GRPSSE(0fd7) },
  /* 0F D8 */ { GRPSSE(0fd8) },
  /* 0F D9 */ { GRPSSE(0fd9) },
  /* 0F DA */ { GRPSSE(0fda) },
  /* 0F DB */ { GRPSSE(0fdb) },
  /* 0F DC */ { GRPSSE(0fdc) },
  /* 0F DD */ { GRPSSE(0fdd) },
  /* 0F DE */ { GRPSSE(0fde) },
  /* 0F DF */ { GRPSSE(0fdf) },
  /* 0F E0 */ { GRPSSE(0fe0) },
  /* 0F E1 */ { GRPSSE(0fe1) },
  /* 0F E2 */ { GRPSSE(0fe2) },
  /* 0F E3 */ { GRPSSE(0fe3) },
  /* 0F E4 */ { GRPSSE(0fe4) },
  /* 0F E5 */ { GRPSSE(0fe5) },
  /* 0F E6 */ { GRPSSE(0fe6) },
  /* 0F E7 */ { GRPSSE(0fe7) },
  /* 0F E8 */ { GRPSSE(0fe8) },
  /* 0F E9 */ { GRPSSE(0fe9) },
  /* 0F EA */ { GRPSSE(0fea) },
  /* 0F EB */ { GRPSSE(0feb) },
  /* 0F EC */ { GRPSSE(0fec) },
  /* 0F ED */ { GRPSSE(0fed) },
  /* 0F EE */ { GRPSSE(0fee) },
  /* 0F EF */ { GRPSSE(0fef) },
  /* 0F F0 */ { GRPSSE(0ff0) },
  /* 0F F1 */ { GRPSSE(0ff1) },
  /* 0F F2 */ { GRPSSE(0ff2) },
  /* 0F F3 */ { GRPSSE(0ff3) },
  /* 0F F4 */ { GRPSSE(0ff4) },
  /* 0F F5 */ { GRPSSE(0ff5) },
  /* 0F F6 */ { GRPSSE(0ff6) },
  /* 0F F7 */ { GRPSSE(0ff7) },
  /* 0F F8 */ { GRPSSE(0ff8) },
  /* 0F F9 */ { GRPSSE(0ff9) },
  /* 0F FA */ { GRPSSE(0ffa) },
  /* 0F FB */ { GRPSSE(0ffb) },
  /* 0F FC */ { GRPSSE(0ffc) },
  /* 0F FD */ { GRPSSE(0ffd) },
  /* 0F FE */ { GRPSSE(0ffe) },
  /* 0F FF */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmOpcodes64d[256*2] = {
  // 256 entries for single byte opcodes
  /* 00 */ { 0, &Ia_addb_Eb_Gb    },
  /* 01 */ { 0, &Ia_addl_Ed_Gd    },
  /* 02 */ { 0, &Ia_addb_Gb_Eb    },
  /* 03 */ { 0, &Ia_addl_Gd_Ed    },
  /* 04 */ { 0, &Ia_addb_AL_Ib,   },
  /* 05 */ { 0, &Ia_addl_EAX_Id,  },
  /* 06 */ { 0, &Ia_Invalid       },
  /* 07 */ { 0, &Ia_Invalid       },
  /* 08 */ { 0, &Ia_orb_Eb_Gb     },
  /* 09 */ { 0, &Ia_orl_Ed_Gd     },
  /* 0A */ { 0, &Ia_orb_Gb_Eb     },
  /* 0B */ { 0, &Ia_orl_Gd_Ed     },
  /* 0C */ { 0, &Ia_orb_AL_Ib     },
  /* 0D */ { 0, &Ia_orl_EAX_Id    },
  /* 0E */ { 0, &Ia_Invalid       },
  /* 0F */ { 0, &Ia_error         },   // 2 byte escape
  /* 10 */ { 0, &Ia_adcb_Eb_Gb    },
  /* 11 */ { 0, &Ia_adcl_Ed_Gd    },
  /* 12 */ { 0, &Ia_adcb_Gb_Eb    },
  /* 13 */ { 0, &Ia_adcl_Gd_Ed    },
  /* 14 */ { 0, &Ia_adcb_AL_Ib    },
  /* 15 */ { 0, &Ia_adcl_EAX_Id   },
  /* 16 */ { 0, &Ia_Invalid       },
  /* 17 */ { 0, &Ia_Invalid       },
  /* 18 */ { 0, &Ia_sbbb_Eb_Gb    },
  /* 19 */ { 0, &Ia_sbbl_Ed_Gd    },
  /* 1A */ { 0, &Ia_sbbb_Gb_Eb    },
  /* 1B */ { 0, &Ia_sbbl_Gd_Ed    },
  /* 1C */ { 0, &Ia_sbbb_AL_Ib    },
  /* 1D */ { 0, &Ia_sbbl_EAX_Id   },
  /* 1E */ { 0, &Ia_Invalid       },
  /* 1F */ { 0, &Ia_Invalid       },
  /* 20 */ { 0, &Ia_andb_Eb_Gb    },
  /* 21 */ { 0, &Ia_andl_Ed_Gd    },
  /* 22 */ { 0, &Ia_andb_Gb_Eb    },
  /* 23 */ { 0, &Ia_andl_Gd_Ed    },
  /* 24 */ { 0, &Ia_andb_AL_Ib    },
  /* 25 */ { 0, &Ia_andl_EAX_Id   },
  /* 26 */ { 0, &Ia_prefix_es     },   // ES:
  /* 27 */ { 0, &Ia_Invalid       },
  /* 28 */ { 0, &Ia_subb_Eb_Gb    },
  /* 29 */ { 0, &Ia_subl_Ed_Gd    },
  /* 2A */ { 0, &Ia_subb_Gb_Eb    },
  /* 2B */ { 0, &Ia_subl_Gd_Ed    },
  /* 2C */ { 0, &Ia_subb_AL_Ib    },
  /* 2D */ { 0, &Ia_subl_EAX_Id   },
  /* 2E */ { 0, &Ia_prefix_cs     },   // CS:
  /* 2F */ { 0, &Ia_Invalid       },
  /* 30 */ { 0, &Ia_xorb_Eb_Gb    },
  /* 31 */ { 0, &Ia_xorl_Ed_Gd    },
  /* 32 */ { 0, &Ia_xorb_Gb_Eb    },
  /* 33 */ { 0, &Ia_xorl_Gd_Ed    },
  /* 34 */ { 0, &Ia_xorb_AL_Ib    },
  /* 35 */ { 0, &Ia_xorl_EAX_Id   },
  /* 36 */ { 0, &Ia_prefix_ss     },   // SS:
  /* 37 */ { 0, &Ia_Invalid       },
  /* 38 */ { 0, &Ia_cmpb_Eb_Gb    },
  /* 39 */ { 0, &Ia_cmpl_Ed_Gd    },
  /* 3A */ { 0, &Ia_cmpb_Gb_Eb    },
  /* 3B */ { 0, &Ia_cmpl_Gd_Ed    },
  /* 3C */ { 0, &Ia_cmpb_AL_Ib    },
  /* 3D */ { 0, &Ia_cmpl_EAX_Id   },
  /* 3E */ { 0, &Ia_prefix_ds     },   // DS:
  /* 3F */ { 0, &Ia_Invalid       },
  /* 40 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 41 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 42 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 43 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 44 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 45 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 46 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 47 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 48 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 49 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4A */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4B */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4C */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4D */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4E */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4F */ { 0, &Ia_prefix_rex    },   // REX:
  /* 50 */ { 0, &Ia_pushq_RRX     },
  /* 51 */ { 0, &Ia_pushq_RRX     },
  /* 52 */ { 0, &Ia_pushq_RRX     },
  /* 53 */ { 0, &Ia_pushq_RRX     },
  /* 54 */ { 0, &Ia_pushq_RRX     },
  /* 55 */ { 0, &Ia_pushq_RRX     },
  /* 56 */ { 0, &Ia_pushq_RRX     },
  /* 57 */ { 0, &Ia_pushq_RRX     },
  /* 58 */ { 0, &Ia_popq_RRX      },
  /* 59 */ { 0, &Ia_popq_RRX      },
  /* 5A */ { 0, &Ia_popq_RRX      },
  /* 5B */ { 0, &Ia_popq_RRX      },
  /* 5C */ { 0, &Ia_popq_RRX      },
  /* 5D */ { 0, &Ia_popq_RRX      },
  /* 5E */ { 0, &Ia_popq_RRX      },
  /* 5F */ { 0, &Ia_popq_RRX      },
  /* 60 */ { 0, &Ia_Invalid       },
  /* 61 */ { 0, &Ia_Invalid       },
  /* 62 */ { 0, &Ia_Invalid       },
  /* 63 */ { 0, &Ia_movl_Gd_Ed    },
  /* 64 */ { 0, &Ia_prefix_fs     },   // FS:
  /* 65 */ { 0, &Ia_prefix_gs     },   // GS:
  /* 66 */ { 0, &Ia_prefix_osize  },   // OSIZE:
  /* 67 */ { 0, &Ia_prefix_asize  },   // ASIZE:
  /* 68 */ { 0, &Ia_pushq_sId     },
  /* 69 */ { 0, &Ia_imull_Gd_Ed_Id  },
  /* 6A */ { 0, &Ia_pushq_sIb     },
  /* 6B */ { 0, &Ia_imull_Gd_Ed_sIb },
  /* 6C */ { 0, &Ia_insb_Yb_DX    },
  /* 6D */ { 0, &Ia_insl_Yd_DX    },
  /* 6E */ { 0, &Ia_outsb_DX_Xb   },
  /* 6F */ { 0, &Ia_outsl_DX_Xd   },
  /* 70 */ { 0, &Ia_jo_Jb         },
  /* 71 */ { 0, &Ia_jno_Jb        },
  /* 72 */ { 0, &Ia_jb_Jb         },
  /* 73 */ { 0, &Ia_jnb_Jb        },
  /* 74 */ { 0, &Ia_jz_Jb         },
  /* 75 */ { 0, &Ia_jnz_Jb        },
  /* 76 */ { 0, &Ia_jbe_Jb        },
  /* 77 */ { 0, &Ia_jnbe_Jb       },
  /* 78 */ { 0, &Ia_js_Jb         },
  /* 79 */ { 0, &Ia_jns_Jb        },
  /* 7A */ { 0, &Ia_jp_Jb         },
  /* 7B */ { 0, &Ia_jnp_Jb        },
  /* 7C */ { 0, &Ia_jl_Jb         },
  /* 7D */ { 0, &Ia_jnl_Jb        },
  /* 7E */ { 0, &Ia_jle_Jb        },
  /* 7F */ { 0, &Ia_jnle_Jb       },
  /* 80 */ { GRPN(G1EbIb)         },
  /* 81 */ { GRPN(G1EdId)         },
  /* 82 */ { 0, &Ia_Invalid       },
  /* 83 */ { GRPN(G1EdIb)         },
  /* 84 */ { 0, &Ia_testb_Eb_Gb   },
  /* 85 */ { 0, &Ia_testl_Ed_Gd   },
  /* 86 */ { 0, &Ia_xchgb_Eb_Gb   },
  /* 87 */ { 0, &Ia_xchgl_Ed_Gd   },
  /* 88 */ { 0, &Ia_movb_Eb_Gb    },
  /* 89 */ { 0, &Ia_movl_Ed_Gd    },
  /* 8A */ { 0, &Ia_movb_Gb_Eb    },
  /* 8B */ { 0, &Ia_movl_Gd_Ed    },
  /* 8C */ { 0, &Ia_movw_Ew_Sw    },
  /* 8D */ { 0, &Ia_leal_Gd_Md    },
  /* 8E */ { 0, &Ia_movw_Sw_Ew    },
  /* 8F */ { 0, &Ia_popq_Eq       },
  /* 90 */ { 0, &Ia_xchgl_ERX_EAX }, // handle XCHG R8d, EAX
  /* 91 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 92 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 93 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 94 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 95 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 96 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 97 */ { 0, &Ia_xchgl_ERX_EAX },
  /* 98 */ { 0, &Ia_cwde          },
  /* 99 */ { 0, &Ia_cdq           },
  /* 9A */ { 0, &Ia_Invalid       },
  /* 9B */ { 0, &Ia_fwait         },
  /* 9C */ { 0, &Ia_pushfq        },
  /* 9D */ { 0, &Ia_popfq         },
  /* 9E */ { 0, &Ia_sahf          },
  /* 9F */ { 0, &Ia_lahf          },
  /* A0 */ { 0, &Ia_movb_AL_Ob    },
  /* A1 */ { 0, &Ia_movl_EAX_Od   },
  /* A0 */ { 0, &Ia_movb_Ob_AL    },
  /* A1 */ { 0, &Ia_movl_Od_EAX   },
  /* A4 */ { 0, &Ia_movsb_Yb_Xb   },
  /* A5 */ { 0, &Ia_movsl_Yd_Xd   },
  /* A6 */ { 0, &Ia_cmpsb_Yb_Xb   },
  /* A7 */ { 0, &Ia_cmpsl_Yd_Xd   },
  /* A8 */ { 0, &Ia_testb_AL_Ib   },
  /* A9 */ { 0, &Ia_testl_EAX_Id  },
  /* AA */ { 0, &Ia_stosb_Yb_AL   },
  /* AB */ { 0, &Ia_stosl_Yd_EAX  },
  /* AC */ { 0, &Ia_lodsb_AL_Xb   },
  /* AD */ { 0, &Ia_lodsl_EAX_Xd  },
  /* AE */ { 0, &Ia_scasb_Yb_AL   },
  /* AF */ { 0, &Ia_scasl_Yd_EAX  },
  /* B0 */ { 0, &Ia_movb_R8_Ib    },
  /* B1 */ { 0, &Ia_movb_R8_Ib    },
  /* B2 */ { 0, &Ia_movb_R8_Ib    },
  /* B3 */ { 0, &Ia_movb_R8_Ib    },
  /* B4 */ { 0, &Ia_movb_R8_Ib    },
  /* B5 */ { 0, &Ia_movb_R8_Ib    },
  /* B6 */ { 0, &Ia_movb_R8_Ib    },
  /* B7 */ { 0, &Ia_movb_R8_Ib    },
  /* B8 */ { 0, &Ia_movl_ERX_Id   },
  /* B9 */ { 0, &Ia_movl_ERX_Id   },
  /* BA */ { 0, &Ia_movl_ERX_Id   },
  /* BB */ { 0, &Ia_movl_ERX_Id   },
  /* BC */ { 0, &Ia_movl_ERX_Id   },
  /* BD */ { 0, &Ia_movl_ERX_Id   },
  /* BE */ { 0, &Ia_movl_ERX_Id   },
  /* BF */ { 0, &Ia_movl_ERX_Id   },
  /* C0 */ { GRPN(G2Eb)           },
  /* C1 */ { GRPN(G2Ed)           },
  /* C2 */ { 0, &Ia_ret_Iw        },
  /* C3 */ { 0, &Ia_ret           },
  /* C4 */ { 0, &Ia_Invalid       },
  /* C5 */ { 0, &Ia_Invalid       },
  /* C6 */ { 0, &Ia_movb_Eb_Ib    },
  /* C7 */ { 0, &Ia_movl_Ed_Id    },
  /* C8 */ { 0, &Ia_enter         },
  /* C9 */ { 0, &Ia_leave         },
  /* CA */ { 0, &Ia_lret_Iw       },
  /* CB */ { 0, &Ia_lret          },
  /* CC */ { 0, &Ia_int3          },
  /* CD */ { 0, &Ia_int_Ib        },
  /* CE */ { 0, &Ia_Invalid       },
  /* CF */ { 0, &Ia_iretl         },
  /* D0 */ { GRPN(G2EbI1)         },
  /* D1 */ { GRPN(G2EdI1)         },
  /* D2 */ { GRPN(G2EbCL)         },
  /* D3 */ { GRPN(G2EdCL)         },
  /* D4 */ { 0, &Ia_Invalid       },
  /* D5 */ { 0, &Ia_Invalid       },
  /* D6 */ { 0, &Ia_Invalid       },
  /* D7 */ { 0, &Ia_xlat          },
  /* D8 */ { GRPFP(D8)            },
  /* D9 */ { GRPFP(D9)            },
  /* DA */ { GRPFP(DA)            },
  /* DB */ { GRPFP(DB)            },
  /* DC */ { GRPFP(DC)            },
  /* DD */ { GRPFP(DD)            },
  /* DE */ { GRPFP(DE)            },
  /* DF */ { GRPFP(DF)            },
  /* E0 */ { 0, &Ia_loopne_Jb     },
  /* E1 */ { 0, &Ia_loope_Jb      },
  /* E2 */ { 0, &Ia_loop_Jb       },
  /* E3 */ { 0, &Ia_jrcxz_Jb      },
  /* E4 */ { 0, &Ia_inb_AL_Ib     },
  /* E5 */ { 0, &Ia_inl_EAX_Ib    },
  /* E6 */ { 0, &Ia_outb_Ib_AL    },
  /* E7 */ { 0, &Ia_outl_Ib_EAX   },
  /* E8 */ { 0, &Ia_call_Jd       },
  /* E9 */ { 0, &Ia_jmp_Jd        },
  /* EA */ { 0, &Ia_Invalid       },
  /* EB */ { 0, &Ia_jmp_Jb        },
  /* EC */ { 0, &Ia_inb_AL_DX     },
  /* ED */ { 0, &Ia_inl_EAX_DX    },
  /* EE */ { 0, &Ia_outb_DX_AL    },
  /* EF */ { 0, &Ia_outl_DX_EAX   },
  /* F0 */ { 0, &Ia_prefix_lock   },   // LOCK:
  /* F1 */ { 0, &Ia_int1          },
  /* F2 */ { 0, &Ia_prefix_repne  },   // REPNE:
  /* F3 */ { 0, &Ia_prefix_rep    },   // REP:
  /* F4 */ { 0, &Ia_hlt           },
  /* F5 */ { 0, &Ia_cmc           },
  /* F6 */ { GRPN(G3Eb)           },
  /* F7 */ { GRPN(G3Ed)           },
  /* F8 */ { 0, &Ia_clc           },
  /* F9 */ { 0, &Ia_stc           },
  /* FA */ { 0, &Ia_cli           },
  /* FB */ { 0, &Ia_sti           },
  /* FC */ { 0, &Ia_cld           },
  /* FD */ { 0, &Ia_std           },
  /* FE */ { GRPN(G4)             },
  /* FF */ { GRPN(64G5d)          },

  // 256 entries for two byte opcodes
  /* 0F 00 */ { GRPN(G6)          },
  /* 0F 01 */ { GRPN(G7)          },
  /* 0F 02 */ { 0, &Ia_larl_Gd_Ew },
  /* 0F 03 */ { 0, &Ia_lsll_Gd_Ew },
  /* 0F 04 */ { 0, &Ia_Invalid    },
  /* 0F 05 */ { 0, &Ia_syscall    },
  /* 0F 06 */ { 0, &Ia_clts       },
  /* 0F 07 */ { 0, &Ia_sysret     },
  /* 0F 08 */ { 0, &Ia_invd       },
  /* 0F 09 */ { 0, &Ia_wbinvd     },
  /* 0F 0A */ { 0, &Ia_Invalid    },
  /* 0F 0B */ { 0, &Ia_ud2a       },
  /* 0F 0C */ { 0, &Ia_Invalid    },
  /* 0F 0D */ { 0, &Ia_prefetch   },   // 3DNow!
  /* 0F 0E */ { 0, &Ia_femms      },   // 3DNow!
  /* 0F 0F */ { GRP3DNOW          },
  /* 0F 10 */ { GRPSSE(0f10)      },
  /* 0F 11 */ { GRPSSE(0f11)      },
  /* 0F 12 */ { GRPSSE(0f12)      },
  /* 0F 13 */ { GRPSSE(0f13)      },
  /* 0F 14 */ { GRPSSE(0f14)      },
  /* 0F 15 */ { GRPSSE(0f15)      },
  /* 0F 16 */ { GRPSSE(0f16)      },
  /* 0F 17 */ { GRPSSE(0f17)      },
  /* 0F 18 */ { GRPN(G16)         },
  /* 0F 19 */ { 0, &Ia_Invalid    },
  /* 0F 1A */ { 0, &Ia_Invalid    },
  /* 0F 1B */ { 0, &Ia_Invalid    },
  /* 0F 1C */ { 0, &Ia_Invalid    },
  /* 0F 1D */ { 0, &Ia_Invalid    },
  /* 0F 1E */ { 0, &Ia_Invalid    },
  /* 0F 1F */ { 0, &Ia_multibyte_nop },
  /* 0F 20 */ { 0, &Ia_movq_Rq_Cq },
  /* 0F 21 */ { 0, &Ia_movq_Rq_Dq },
  /* 0F 22 */ { 0, &Ia_movq_Cq_Rq },
  /* 0F 23 */ { 0, &Ia_movq_Dq_Rq },
  /* 0F 24 */ { 0, &Ia_Invalid    },
  /* 0F 25 */ { 0, &Ia_Invalid    },
  /* 0F 26 */ { 0, &Ia_Invalid    },
  /* 0F 27 */ { 0, &Ia_Invalid    },
  /* 0F 28 */ { GRPSSE(0f28) },
  /* 0F 29 */ { GRPSSE(0f29) },
  /* 0F 2A */ { GRPSSE(0f2a) },
  /* 0F 2B */ { GRPSSE(0f2b) },
  /* 0F 2C */ { GRPSSE(0f2c) },
  /* 0F 2D */ { GRPSSE(0f2d) },
  /* 0F 2E */ { GRPSSE(0f2e) },
  /* 0F 2F */ { GRPSSE(0f2f) },
  /* 0F 30 */ { 0, &Ia_wrmsr },
  /* 0F 31 */ { 0, &Ia_rdtsc },
  /* 0F 32 */ { 0, &Ia_rdmsr },
  /* 0F 33 */ { 0, &Ia_rdpmc },
  /* 0F 34 */ { 0, &Ia_sysenter },
  /* 0F 35 */ { 0, &Ia_sysexit  },
  /* 0F 36 */ { 0, &Ia_Invalid  },
  /* 0F 37 */ { 0, &Ia_Invalid  },
  /* 0F 38 */ { GR3BTAB(A4) },
  /* 0F 39 */ { 0, &Ia_Invalid  },
  /* 0F 3A */ { GR3BTAB(A5) },
  /* 0F 3B */ { 0, &Ia_Invalid  },
  /* 0F 3C */ { 0, &Ia_Invalid  },
  /* 0F 3D */ { 0, &Ia_Invalid  },
  /* 0F 3E */ { 0, &Ia_Invalid  },
  /* 0F 3F */ { 0, &Ia_Invalid  },
  /* 0F 40 */ { 0, &Ia_cmovol_Gd_Ed  },
  /* 0F 41 */ { 0, &Ia_cmovnol_Gd_Ed },
  /* 0F 42 */ { 0, &Ia_cmovcl_Gd_Ed  },
  /* 0F 43 */ { 0, &Ia_cmovncl_Gd_Ed },
  /* 0F 44 */ { 0, &Ia_cmovzl_Gd_Ed  },
  /* 0F 45 */ { 0, &Ia_cmovnzl_Gd_Ed },
  /* 0F 46 */ { 0, &Ia_cmovnal_Gd_Ed },
  /* 0F 47 */ { 0, &Ia_cmoval_Gd_Ed  },
  /* 0F 48 */ { 0, &Ia_cmovsl_Gd_Ed  },
  /* 0F 49 */ { 0, &Ia_cmovnsl_Gd_Ed },
  /* 0F 4A */ { 0, &Ia_cmovpl_Gd_Ed  },
  /* 0F 4B */ { 0, &Ia_cmovnpl_Gd_Ed },
  /* 0F 4C */ { 0, &Ia_cmovll_Gd_Ed  },
  /* 0F 4D */ { 0, &Ia_cmovnll_Gd_Ed },
  /* 0F 4E */ { 0, &Ia_cmovngl_Gd_Ed },
  /* 0F 4F */ { 0, &Ia_cmovgl_Gd_Ed  },
  /* 0F 50 */ { GRPSSE(0f50) },
  /* 0F 51 */ { GRPSSE(0f51) },
  /* 0F 52 */ { GRPSSE(0f52) },
  /* 0F 53 */ { GRPSSE(0f53) },
  /* 0F 54 */ { GRPSSE(0f54) },
  /* 0F 55 */ { GRPSSE(0f55) },
  /* 0F 56 */ { GRPSSE(0f56) },
  /* 0F 57 */ { GRPSSE(0f57) },
  /* 0F 58 */ { GRPSSE(0f58) },
  /* 0F 59 */ { GRPSSE(0f59) },
  /* 0F 5A */ { GRPSSE(0f5a) },
  /* 0F 5B */ { GRPSSE(0f5b) },
  /* 0F 5C */ { GRPSSE(0f5c) },
  /* 0F 5D */ { GRPSSE(0f5d) },
  /* 0F 5E */ { GRPSSE(0f5e) },
  /* 0F 5F */ { GRPSSE(0f5f) },
  /* 0F 60 */ { GRPSSE(0f60) },
  /* 0F 61 */ { GRPSSE(0f61) },
  /* 0F 62 */ { GRPSSE(0f62) },
  /* 0F 63 */ { GRPSSE(0f63) },
  /* 0F 64 */ { GRPSSE(0f64) },
  /* 0F 65 */ { GRPSSE(0f65) },
  /* 0F 66 */ { GRPSSE(0f66) },
  /* 0F 67 */ { GRPSSE(0f67) },
  /* 0F 68 */ { GRPSSE(0f68) },
  /* 0F 69 */ { GRPSSE(0f69) },
  /* 0F 6A */ { GRPSSE(0f6a) },
  /* 0F 6B */ { GRPSSE(0f6b) },
  /* 0F 6C */ { GRPSSE(0f6c) },
  /* 0F 6D */ { GRPSSE(0f6d) },
  /* 0F 6E */ { GRPSSE(0f6e) },
  /* 0F 6F */ { GRPSSE(0f6f) },
  /* 0F 70 */ { GRPSSE(0f70) },
  /* 0F 71 */ { GRPN(G12)    },
  /* 0F 72 */ { GRPN(G13)    },
  /* 0F 73 */ { GRPN(G14)    },
  /* 0F 74 */ { GRPSSE(0f74) },
  /* 0F 75 */ { GRPSSE(0f75) },
  /* 0F 76 */ { GRPSSE(0f76) },
  /* 0F 77 */ { 0, &Ia_emms  },
  /* 0F 78 */ { 0, &Ia_Invalid },
  /* 0F 79 */ { 0, &Ia_Invalid },
  /* 0F 7A */ { 0, &Ia_Invalid },
  /* 0F 7B */ { 0, &Ia_Invalid },
  /* 0F 7C */ { GRPSSE(0f7c) },
  /* 0F 7D */ { GRPSSE(0f7d) },
  /* 0F 7E */ { GRPSSE(0f7e) },
  /* 0F 7F */ { GRPSSE(0f7f) },
  /* 0F 80 */ { 0, &Ia_jo_Jd     },
  /* 0F 81 */ { 0, &Ia_jno_Jd    },
  /* 0F 82 */ { 0, &Ia_jb_Jd     },
  /* 0F 83 */ { 0, &Ia_jnb_Jd    },
  /* 0F 84 */ { 0, &Ia_jz_Jd     },
  /* 0F 85 */ { 0, &Ia_jnz_Jd    },
  /* 0F 86 */ { 0, &Ia_jbe_Jd    },
  /* 0F 87 */ { 0, &Ia_jnbe_Jd   },
  /* 0F 88 */ { 0, &Ia_js_Jd     },
  /* 0F 89 */ { 0, &Ia_jns_Jd    },
  /* 0F 8A */ { 0, &Ia_jp_Jd     },
  /* 0F 8B */ { 0, &Ia_jnp_Jd    },
  /* 0F 8C */ { 0, &Ia_jl_Jd     },
  /* 0F 8D */ { 0, &Ia_jnl_Jd    },
  /* 0F 8E */ { 0, &Ia_jle_Jd    },
  /* 0F 8F */ { 0, &Ia_jnle_Jd   },
  /* 0F 90 */ { 0, &Ia_seto_Eb   },
  /* 0F 91 */ { 0, &Ia_setno_Eb  },
  /* 0F 92 */ { 0, &Ia_setb_Eb   },
  /* 0F 93 */ { 0, &Ia_setnb_Eb  },
  /* 0F 94 */ { 0, &Ia_setz_Eb   },
  /* 0F 95 */ { 0, &Ia_setnz_Eb  },
  /* 0F 96 */ { 0, &Ia_setbe_Eb  },
  /* 0F 97 */ { 0, &Ia_setnbe_Eb },
  /* 0F 98 */ { 0, &Ia_sets_Eb   },
  /* 0F 99 */ { 0, &Ia_setns_Eb  },
  /* 0F 9A */ { 0, &Ia_setp_Eb   },
  /* 0F 9B */ { 0, &Ia_setnp_Eb  },
  /* 0F 9C */ { 0, &Ia_setl_Eb   },
  /* 0F 9D */ { 0, &Ia_setnl_Eb  },
  /* 0F 9E */ { 0, &Ia_setle_Eb  },
  /* 0F 9F */ { 0, &Ia_setnle_Eb },
  /* 0F A0 */ { 0, &Ia_pushq_FS  },
  /* 0F A1 */ { 0, &Ia_popq_FS   },
  /* 0F A2 */ { 0, &Ia_cpuid     },
  /* 0F A3 */ { 0, &Ia_btl_Ed_Gd },
  /* 0F A4 */ { 0, &Ia_shldl_Ed_Gd_Ib },
  /* 0F A5 */ { 0, &Ia_shldl_Ed_Gd_CL },
  /* 0F A6 */ { 0, &Ia_Invalid    },
  /* 0F A7 */ { 0, &Ia_Invalid    },
  /* 0F A8 */ { 0, &Ia_pushq_GS   },
  /* 0F A9 */ { 0, &Ia_popq_GS    },
  /* 0F AA */ { 0, &Ia_rsm        },
  /* 0F AB */ { 0, &Ia_btsl_Ed_Gd },
  /* 0F AC */ { 0, &Ia_shrdl_Ed_Gd_Ib },
  /* 0F AD */ { 0, &Ia_shrdl_Ed_Gd_CL },
  /* 0F AE */ { GRPN(G15)         },
  /* 0F AF */ { 0, &Ia_imull_Gd_Ed    },
  /* 0F B0 */ { 0, &Ia_cmpxchgb_Eb_Gb },
  /* 0F B1 */ { 0, &Ia_cmpxchgl_Ed_Gd },
  /* 0F B2 */ { 0, &Ia_lssl_Gd_Mp   },
  /* 0F B3 */ { 0, &Ia_btrl_Ed_Gd   },
  /* 0F B4 */ { 0, &Ia_lfsl_Gd_Mp   },
  /* 0F B5 */ { 0, &Ia_lgsl_Gd_Mp   },
  /* 0F B6 */ { 0, &Ia_movzbl_Gd_Eb },
  /* 0F B7 */ { 0, &Ia_movzwl_Gd_Ew },
  /* 0F B8 */ { 0, &Ia_Invalid      },
  /* 0F B9 */ { 0, &Ia_ud2b         },
  /* 0F BA */ { GRPN(G8EdIb)        },
  /* 0F BB */ { 0, &Ia_btcl_Ed_Gd   },
  /* 0F BC */ { 0, &Ia_bsfl_Gd_Ed   },
  /* 0F BD */ { 0, &Ia_bsrl_Gd_Ed   },
  /* 0F BE */ { 0, &Ia_movsbl_Gd_Eb },
  /* 0F BF */ { 0, &Ia_movswl_Gd_Ew },
  /* 0F C0 */ { 0, &Ia_xaddb_Eb_Gb  },
  /* 0F C0 */ { 0, &Ia_xaddl_Ed_Gd  },
  /* 0F C2 */ { GRPSSE(0fc2)      },
  /* 0F C3 */ { GRPSSE(0fc3)      },
  /* 0F C4 */ { GRPSSE(0fc4)      },
  /* 0F C5 */ { GRPSSE(0fc5)      },
  /* 0F C6 */ { GRPSSE(0fc6)      },
  /* 0F C7 */ { GRPN(G9)          },
  /* 0F C8 */ { 0, &Ia_bswapl_ERX },
  /* 0F C9 */ { 0, &Ia_bswapl_ERX },
  /* 0F CA */ { 0, &Ia_bswapl_ERX },
  /* 0F CB */ { 0, &Ia_bswapl_ERX },
  /* 0F CC */ { 0, &Ia_bswapl_ERX },
  /* 0F CD */ { 0, &Ia_bswapl_ERX },
  /* 0F CE */ { 0, &Ia_bswapl_ERX },
  /* 0F CF */ { 0, &Ia_bswapl_ERX },
  /* 0F D0 */ { GRPSSE(0fd0) },
  /* 0F D1 */ { GRPSSE(0fd1) },
  /* 0F D2 */ { GRPSSE(0fd2) },
  /* 0F D3 */ { GRPSSE(0fd3) },
  /* 0F D4 */ { GRPSSE(0fd4) },
  /* 0F D5 */ { GRPSSE(0fd5) },
  /* 0F D6 */ { GRPSSE(0fd6) },
  /* 0F D7 */ { GRPSSE(0fd7) },
  /* 0F D8 */ { GRPSSE(0fd8) },
  /* 0F D9 */ { GRPSSE(0fd9) },
  /* 0F DA */ { GRPSSE(0fda) },
  /* 0F DB */ { GRPSSE(0fdb) },
  /* 0F DC */ { GRPSSE(0fdc) },
  /* 0F DD */ { GRPSSE(0fdd) },
  /* 0F DE */ { GRPSSE(0fde) },
  /* 0F DF */ { GRPSSE(0fdf) },
  /* 0F E0 */ { GRPSSE(0fe0) },
  /* 0F E1 */ { GRPSSE(0fe1) },
  /* 0F E2 */ { GRPSSE(0fe2) },
  /* 0F E3 */ { GRPSSE(0fe3) },
  /* 0F E4 */ { GRPSSE(0fe4) },
  /* 0F E5 */ { GRPSSE(0fe5) },
  /* 0F E6 */ { GRPSSE(0fe6) },
  /* 0F E7 */ { GRPSSE(0fe7) },
  /* 0F E8 */ { GRPSSE(0fe8) },
  /* 0F E9 */ { GRPSSE(0fe9) },
  /* 0F EA */ { GRPSSE(0fea) },
  /* 0F EB */ { GRPSSE(0feb) },
  /* 0F EC */ { GRPSSE(0fec) },
  /* 0F ED */ { GRPSSE(0fed) },
  /* 0F EE */ { GRPSSE(0fee) },
  /* 0F EF */ { GRPSSE(0fef) },
  /* 0F F0 */ { GRPSSE(0ff0) },
  /* 0F F1 */ { GRPSSE(0ff1) },
  /* 0F F2 */ { GRPSSE(0ff2) },
  /* 0F F3 */ { GRPSSE(0ff3) },
  /* 0F F4 */ { GRPSSE(0ff4) },
  /* 0F F5 */ { GRPSSE(0ff5) },
  /* 0F F6 */ { GRPSSE(0ff6) },
  /* 0F F7 */ { GRPSSE(0ff7) },
  /* 0F F8 */ { GRPSSE(0ff8) },
  /* 0F F9 */ { GRPSSE(0ff9) },
  /* 0F FA */ { GRPSSE(0ffa) },
  /* 0F FB */ { GRPSSE(0ffb) },
  /* 0F FC */ { GRPSSE(0ffc) },
  /* 0F FD */ { GRPSSE(0ffd) },
  /* 0F FE */ { GRPSSE(0ffe) },
  /* 0F FF */ { 0, &Ia_Invalid }
};

static BxDisasmOpcodeTable_t BxDisasmOpcodes64q[256*2] = {
  // 256 entries for single byte opcodes
  /* 00 */ { 0, &Ia_addb_Eb_Gb    },
  /* 01 */ { 0, &Ia_addq_Eq_Gq    },
  /* 02 */ { 0, &Ia_addb_Gb_Eb    },
  /* 03 */ { 0, &Ia_addq_Gq_Eq    },
  /* 04 */ { 0, &Ia_addb_AL_Ib,   },
  /* 05 */ { 0, &Ia_addq_RAX_sId  },
  /* 06 */ { 0, &Ia_Invalid       },
  /* 07 */ { 0, &Ia_Invalid       },
  /* 08 */ { 0, &Ia_orb_Eb_Gb     },
  /* 09 */ { 0, &Ia_orq_Eq_Gq     },
  /* 0A */ { 0, &Ia_orb_Gb_Eb     },
  /* 0B */ { 0, &Ia_orq_Gq_Eq     },
  /* 0C */ { 0, &Ia_orb_AL_Ib     },
  /* 0D */ { 0, &Ia_orq_RAX_sId   },
  /* 0E */ { 0, &Ia_Invalid       },
  /* 0F */ { 0, &Ia_error         },   // 2 byte escape
  /* 10 */ { 0, &Ia_adcb_Eb_Gb    },
  /* 11 */ { 0, &Ia_adcq_Eq_Gq    },
  /* 12 */ { 0, &Ia_adcb_Gb_Eb    },
  /* 13 */ { 0, &Ia_adcq_Gq_Eq    },
  /* 14 */ { 0, &Ia_adcb_AL_Ib    },
  /* 15 */ { 0, &Ia_adcq_RAX_sId  },
  /* 16 */ { 0, &Ia_Invalid       },
  /* 17 */ { 0, &Ia_Invalid       },
  /* 18 */ { 0, &Ia_sbbb_Eb_Gb    },
  /* 19 */ { 0, &Ia_sbbq_Eq_Gq    },
  /* 1A */ { 0, &Ia_sbbb_Gb_Eb    },
  /* 1B */ { 0, &Ia_sbbq_Gq_Eq    },
  /* 1C */ { 0, &Ia_sbbb_AL_Ib    },
  /* 1D */ { 0, &Ia_sbbq_RAX_sId  },
  /* 1E */ { 0, &Ia_Invalid       },
  /* 1F */ { 0, &Ia_Invalid       },
  /* 20 */ { 0, &Ia_andb_Eb_Gb    },
  /* 21 */ { 0, &Ia_andq_Eq_Gq    },
  /* 22 */ { 0, &Ia_andb_Gb_Eb    },
  /* 23 */ { 0, &Ia_andq_Gq_Eq    },
  /* 24 */ { 0, &Ia_andb_AL_Ib    },
  /* 25 */ { 0, &Ia_andq_RAX_sId  },
  /* 26 */ { 0, &Ia_prefix_es     },   // ES:
  /* 27 */ { 0, &Ia_Invalid       },
  /* 28 */ { 0, &Ia_subb_Eb_Gb    },
  /* 29 */ { 0, &Ia_subq_Eq_Gq    },
  /* 2A */ { 0, &Ia_subb_Gb_Eb    },
  /* 2B */ { 0, &Ia_subq_Gq_Eq    },
  /* 2C */ { 0, &Ia_subb_AL_Ib    },
  /* 2D */ { 0, &Ia_subq_RAX_sId  },
  /* 2E */ { 0, &Ia_prefix_cs     },   // CS:
  /* 2F */ { 0, &Ia_Invalid       },
  /* 30 */ { 0, &Ia_xorb_Eb_Gb    },
  /* 31 */ { 0, &Ia_xorq_Eq_Gq    },
  /* 32 */ { 0, &Ia_xorb_Gb_Eb    },
  /* 33 */ { 0, &Ia_xorq_Gq_Eq    },
  /* 34 */ { 0, &Ia_xorb_AL_Ib    },
  /* 35 */ { 0, &Ia_xorq_RAX_sId  },
  /* 36 */ { 0, &Ia_prefix_ss     },   // SS:
  /* 37 */ { 0, &Ia_Invalid       },
  /* 38 */ { 0, &Ia_cmpb_Eb_Gb    },
  /* 39 */ { 0, &Ia_cmpq_Eq_Gq    },
  /* 3A */ { 0, &Ia_cmpb_Gb_Eb    },
  /* 3B */ { 0, &Ia_cmpq_Gq_Eq    },
  /* 3C */ { 0, &Ia_cmpb_AL_Ib    },
  /* 3D */ { 0, &Ia_cmpq_RAX_sId  },
  /* 3E */ { 0, &Ia_prefix_ds     },   // DS:
  /* 3F */ { 0, &Ia_Invalid       },
  /* 40 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 41 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 42 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 43 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 44 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 45 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 46 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 47 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 48 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 49 */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4A */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4B */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4C */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4D */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4E */ { 0, &Ia_prefix_rex    },   // REX:
  /* 4F */ { 0, &Ia_prefix_rex    },   // REX:
  /* 50 */ { 0, &Ia_pushq_RRX     },
  /* 51 */ { 0, &Ia_pushq_RRX     },
  /* 52 */ { 0, &Ia_pushq_RRX     },
  /* 53 */ { 0, &Ia_pushq_RRX     },
  /* 54 */ { 0, &Ia_pushq_RRX     },
  /* 55 */ { 0, &Ia_pushq_RRX     },
  /* 56 */ { 0, &Ia_pushq_RRX     },
  /* 57 */ { 0, &Ia_pushq_RRX     },
  /* 58 */ { 0, &Ia_popq_RRX      },
  /* 59 */ { 0, &Ia_popq_RRX      },
  /* 5A */ { 0, &Ia_popq_RRX      },
  /* 5B */ { 0, &Ia_popq_RRX      },
  /* 5C */ { 0, &Ia_popq_RRX      },
  /* 5D */ { 0, &Ia_popq_RRX      },
  /* 5E */ { 0, &Ia_popq_RRX      },
  /* 5F */ { 0, &Ia_popq_RRX      },
  /* 60 */ { 0, &Ia_Invalid       },
  /* 61 */ { 0, &Ia_Invalid       },
  /* 62 */ { 0, &Ia_Invalid       },
  /* 63 */ { 0, &Ia_movslq_Gq_Ed  },
  /* 64 */ { 0, &Ia_prefix_fs     },   // FS:
  /* 65 */ { 0, &Ia_prefix_gs     },   // GS:
  /* 66 */ { 0, &Ia_prefix_osize  },   // OSIZE:
  /* 67 */ { 0, &Ia_prefix_asize  },   // ASIZE:
  /* 68 */ { 0, &Ia_pushq_sId     },
  /* 69 */ { 0, &Ia_imulq_Gq_Eq_sId },
  /* 6A */ { 0, &Ia_pushq_sIb     },
  /* 6B */ { 0, &Ia_imulq_Gq_Eq_sIb },
  /* 6C */ { 0, &Ia_insb_Yb_DX    },
  /* 6D */ { 0, &Ia_insl_Yd_DX    },
  /* 6E */ { 0, &Ia_outsb_DX_Xb   },
  /* 6F */ { 0, &Ia_outsl_DX_Xd   },
  /* 70 */ { 0, &Ia_jo_Jb         },
  /* 71 */ { 0, &Ia_jno_Jb        },
  /* 72 */ { 0, &Ia_jb_Jb         },
  /* 73 */ { 0, &Ia_jnb_Jb        },
  /* 74 */ { 0, &Ia_jz_Jb         },
  /* 75 */ { 0, &Ia_jnz_Jb        },
  /* 76 */ { 0, &Ia_jbe_Jb        },
  /* 77 */ { 0, &Ia_jnbe_Jb       },
  /* 78 */ { 0, &Ia_js_Jb         },
  /* 79 */ { 0, &Ia_jns_Jb        },
  /* 7A */ { 0, &Ia_jp_Jb         },
  /* 7B */ { 0, &Ia_jnp_Jb        },
  /* 7C */ { 0, &Ia_jl_Jb         },
  /* 7D */ { 0, &Ia_jnl_Jb        },
  /* 7E */ { 0, &Ia_jle_Jb        },
  /* 7F */ { 0, &Ia_jnle_Jb       },
  /* 80 */ { GRPN(G1EbIb)         },
  /* 81 */ { GRPN(G1EqId)         },
  /* 82 */ { 0, &Ia_Invalid       },
  /* 83 */ { GRPN(G1EqIb)         },
  /* 84 */ { 0, &Ia_testb_Eb_Gb   },
  /* 85 */ { 0, &Ia_testq_Eq_Gq   },
  /* 86 */ { 0, &Ia_xchgb_Eb_Gb   },
  /* 87 */ { 0, &Ia_xchgq_Eq_Gq   },
  /* 88 */ { 0, &Ia_movb_Eb_Gb    },
  /* 89 */ { 0, &Ia_movq_Eq_Gq    },
  /* 8A */ { 0, &Ia_movb_Gb_Eb    },
  /* 8B */ { 0, &Ia_movq_Gq_Eq    },
  /* 8C */ { 0, &Ia_movw_Ew_Sw    },
  /* 8D */ { 0, &Ia_leaq_Gq_Mq    },
  /* 8E */ { 0, &Ia_movw_Sw_Ew    },
  /* 8F */ { 0, &Ia_popq_Eq       },
  /* 90 */ { 0, &Ia_xchgq_RRX_RAX }, // handle XCHG R8, RAX
  /* 91 */ { 0, &Ia_xchgq_RRX_RAX },
  /* 92 */ { 0, &Ia_xchgq_RRX_RAX },
  /* 93 */ { 0, &Ia_xchgq_RRX_RAX },
  /* 94 */ { 0, &Ia_xchgq_RRX_RAX },
  /* 95 */ { 0, &Ia_xchgq_RRX_RAX },
  /* 96 */ { 0, &Ia_xchgq_RRX_RAX },
  /* 97 */ { 0, &Ia_xchgq_RRX_RAX },
  /* 98 */ { 0, &Ia_cdqe          },
  /* 99 */ { 0, &Ia_cqo           },
  /* 9A */ { 0, &Ia_Invalid       },
  /* 9B */ { 0, &Ia_fwait         },
  /* 9C */ { 0, &Ia_pushfq        },
  /* 9D */ { 0, &Ia_popfq         },
  /* 9E */ { 0, &Ia_sahf          },
  /* 9F */ { 0, &Ia_lahf          },
  /* A0 */ { 0, &Ia_movb_AL_Ob    },
  /* A1 */ { 0, &Ia_movq_RAX_Oq   },
  /* A0 */ { 0, &Ia_movb_Ob_AL    },
  /* A1 */ { 0, &Ia_movq_Oq_RAX   },
  /* A4 */ { 0, &Ia_movsb_Yb_Xb   },
  /* A5 */ { 0, &Ia_movsq_Yq_Xq   },
  /* A6 */ { 0, &Ia_cmpsb_Yb_Xb   },
  /* A7 */ { 0, &Ia_cmpsq_Yq_Xq   },
  /* A8 */ { 0, &Ia_testb_AL_Ib   },
  /* A9 */ { 0, &Ia_testq_RAX_sId },
  /* AA */ { 0, &Ia_stosb_Yb_AL   },
  /* AB */ { 0, &Ia_stosq_Yq_RAX  },
  /* AC */ { 0, &Ia_lodsb_AL_Xb   },
  /* AD */ { 0, &Ia_lodsq_RAX_Xq  },
  /* AE */ { 0, &Ia_scasb_Yb_AL   },
  /* AF */ { 0, &Ia_scasq_Yq_RAX  },
  /* B0 */ { 0, &Ia_movb_R8_Ib    },
  /* B1 */ { 0, &Ia_movb_R8_Ib    },
  /* B2 */ { 0, &Ia_movb_R8_Ib    },
  /* B3 */ { 0, &Ia_movb_R8_Ib    },
  /* B4 */ { 0, &Ia_movb_R8_Ib    },
  /* B5 */ { 0, &Ia_movb_R8_Ib    },
  /* B6 */ { 0, &Ia_movb_R8_Ib    },
  /* B7 */ { 0, &Ia_movb_R8_Ib    },
  /* B8 */ { 0, &Ia_movq_RRX_Iq   },
  /* B9 */ { 0, &Ia_movq_RRX_Iq   },
  /* BA */ { 0, &Ia_movq_RRX_Iq   },
  /* BB */ { 0, &Ia_movq_RRX_Iq   },
  /* BC */ { 0, &Ia_movq_RRX_Iq   },
  /* BD */ { 0, &Ia_movq_RRX_Iq   },
  /* BE */ { 0, &Ia_movq_RRX_Iq   },
  /* BF */ { 0, &Ia_movq_RRX_Iq   },
  /* C0 */ { GRPN(G2Eb)           },
  /* C1 */ { GRPN(G2Eq)           },
  /* C2 */ { 0, &Ia_ret_Iw        },
  /* C3 */ { 0, &Ia_ret           },
  /* C4 */ { 0, &Ia_Invalid       },
  /* C5 */ { 0, &Ia_Invalid       },
  /* C6 */ { 0, &Ia_movb_Eb_Ib    },
  /* C7 */ { 0, &Ia_movq_Eq_sId   },
  /* C8 */ { 0, &Ia_enter         },
  /* C9 */ { 0, &Ia_leave         },
  /* CA */ { 0, &Ia_lret_Iw       },
  /* CB */ { 0, &Ia_lret          },
  /* CC */ { 0, &Ia_int3          },
  /* CD */ { 0, &Ia_int_Ib        },
  /* CE */ { 0, &Ia_Invalid       },
  /* CF */ { 0, &Ia_iretq         },
  /* D0 */ { GRPN(G2EbI1)         },
  /* D1 */ { GRPN(G2EqI1)         },
  /* D2 */ { GRPN(G2EbCL)         },
  /* D3 */ { GRPN(G2EqCL)         },
  /* D4 */ { 0, &Ia_Invalid       },
  /* D5 */ { 0, &Ia_Invalid       },
  /* D6 */ { 0, &Ia_Invalid       },
  /* D7 */ { 0, &Ia_xlat          },
  /* D8 */ { GRPFP(D8)            },
  /* D9 */ { GRPFP(D9)            },
  /* DA */ { GRPFP(DA)            },
  /* DB */ { GRPFP(DB)            },
  /* DC */ { GRPFP(DC)            },
  /* DD */ { GRPFP(DD)            },
  /* DE */ { GRPFP(DE)            },
  /* DF */ { GRPFP(DF)            },
  /* E0 */ { 0, &Ia_loopne_Jb     },
  /* E1 */ { 0, &Ia_loope_Jb      },
  /* E2 */ { 0, &Ia_loop_Jb       },
  /* E3 */ { 0, &Ia_jrcxz_Jb      },
  /* E4 */ { 0, &Ia_inb_AL_Ib     },
  /* E5 */ { 0, &Ia_inl_EAX_Ib    },
  /* E6 */ { 0, &Ia_outb_Ib_AL    },
  /* E7 */ { 0, &Ia_outl_Ib_EAX   },
  /* E8 */ { 0, &Ia_call_Jd       },
  /* E9 */ { 0, &Ia_jmp_Jd        },
  /* EA */ { 0, &Ia_Invalid       },
  /* EB */ { 0, &Ia_jmp_Jb        },
  /* EC */ { 0, &Ia_inb_AL_DX     },
  /* ED */ { 0, &Ia_inl_EAX_DX    },
  /* EE */ { 0, &Ia_outb_DX_AL    },
  /* EF */ { 0, &Ia_outl_DX_EAX   },
  /* F0 */ { 0, &Ia_prefix_lock   },   // LOCK:
  /* F1 */ { 0, &Ia_int1          },
  /* F2 */ { 0, &Ia_prefix_repne  },   // REPNE:
  /* F3 */ { 0, &Ia_prefix_rep    },   // REP:
  /* F4 */ { 0, &Ia_hlt           },
  /* F5 */ { 0, &Ia_cmc           },
  /* F6 */ { GRPN(G3Eb)           },
  /* F7 */ { GRPN(G3Eq)           },
  /* F8 */ { 0, &Ia_clc           },
  /* F9 */ { 0, &Ia_stc           },
  /* FA */ { 0, &Ia_cli           },
  /* FB */ { 0, &Ia_sti           },
  /* FC */ { 0, &Ia_cld           },
  /* FD */ { 0, &Ia_std           },
  /* FE */ { GRPN(G4)             },
  /* FF */ { GRPN(64G5q)          },

  // 256 entries for two byte opcodes
  /* 0F 00 */ { GRPN(G6)          },
  /* 0F 01 */ { GRPN(G7)          },
  /* 0F 02 */ { 0, &Ia_larq_Gq_Ew },
  /* 0F 03 */ { 0, &Ia_lslq_Gq_Ew },
  /* 0F 04 */ { 0, &Ia_Invalid    },
  /* 0F 05 */ { 0, &Ia_syscall    },
  /* 0F 06 */ { 0, &Ia_clts       },
  /* 0F 07 */ { 0, &Ia_sysret     },
  /* 0F 08 */ { 0, &Ia_invd       },
  /* 0F 09 */ { 0, &Ia_wbinvd     },
  /* 0F 0A */ { 0, &Ia_Invalid    },
  /* 0F 0B */ { 0, &Ia_ud2a       },
  /* 0F 0C */ { 0, &Ia_Invalid    },
  /* 0F 0D */ { 0, &Ia_prefetch   },   // 3DNow!
  /* 0F 0E */ { 0, &Ia_femms      },   // 3DNow!
  /* 0F 0F */ { GRP3DNOW          },
  /* 0F 10 */ { GRPSSE(0f10)      },
  /* 0F 11 */ { GRPSSE(0f11)      },
  /* 0F 12 */ { GRPSSE(0f12)      },
  /* 0F 13 */ { GRPSSE(0f13)      },
  /* 0F 14 */ { GRPSSE(0f14)      },
  /* 0F 15 */ { GRPSSE(0f15)      },
  /* 0F 16 */ { GRPSSE(0f16)      },
  /* 0F 17 */ { GRPSSE(0f17)      },
  /* 0F 18 */ { GRPN(G16)         },
  /* 0F 19 */ { 0, &Ia_Invalid    },
  /* 0F 1A */ { 0, &Ia_Invalid    },
  /* 0F 1B */ { 0, &Ia_Invalid    },
  /* 0F 1C */ { 0, &Ia_Invalid    },
  /* 0F 1D */ { 0, &Ia_Invalid    },
  /* 0F 1E */ { 0, &Ia_Invalid    },
  /* 0F 1F */ { 0, &Ia_multibyte_nop },
  /* 0F 20 */ { 0, &Ia_movq_Rq_Cq },
  /* 0F 21 */ { 0, &Ia_movq_Rq_Dq },
  /* 0F 22 */ { 0, &Ia_movq_Cq_Rq },
  /* 0F 23 */ { 0, &Ia_movq_Dq_Rq },
  /* 0F 24 */ { 0, &Ia_Invalid    },
  /* 0F 25 */ { 0, &Ia_Invalid    },
  /* 0F 26 */ { 0, &Ia_Invalid    },
  /* 0F 27 */ { 0, &Ia_Invalid    },
  /* 0F 28 */ { GRPSSE(0f28) },
  /* 0F 29 */ { GRPSSE(0f29) },
  /* 0F 2A */ { GRPSSE(640f2a) },
  /* 0F 2B */ { GRPSSE(0f2b) },
  /* 0F 2C */ { GRPSSE(0f2cQ) },
  /* 0F 2D */ { GRPSSE(0f2dQ) },
  /* 0F 2E */ { GRPSSE(0f2e) },
  /* 0F 2F */ { GRPSSE(0f2f) },
  /* 0F 30 */ { 0, &Ia_wrmsr },
  /* 0F 31 */ { 0, &Ia_rdtsc },
  /* 0F 32 */ { 0, &Ia_rdmsr },
  /* 0F 33 */ { 0, &Ia_rdpmc },
  /* 0F 34 */ { 0, &Ia_sysenter },
  /* 0F 35 */ { 0, &Ia_sysexit  },
  /* 0F 36 */ { 0, &Ia_Invalid  },
  /* 0F 37 */ { 0, &Ia_Invalid  },
  /* 0F 38 */ { GR3BTAB(A4) },
  /* 0F 39 */ { 0, &Ia_Invalid  },
  /* 0F 3A */ { GR3BTAB(A5) },
  /* 0F 3B */ { 0, &Ia_Invalid  },
  /* 0F 3C */ { 0, &Ia_Invalid  },
  /* 0F 3D */ { 0, &Ia_Invalid  },
  /* 0F 3E */ { 0, &Ia_Invalid  },
  /* 0F 3F */ { 0, &Ia_Invalid  },
  /* 0F 40 */ { 0, &Ia_cmovoq_Gq_Eq  },
  /* 0F 41 */ { 0, &Ia_cmovnoq_Gq_Eq },
  /* 0F 42 */ { 0, &Ia_cmovcq_Gq_Eq  },
  /* 0F 43 */ { 0, &Ia_cmovncq_Gq_Eq },
  /* 0F 44 */ { 0, &Ia_cmovzq_Gq_Eq  },
  /* 0F 45 */ { 0, &Ia_cmovnzq_Gq_Eq },
  /* 0F 46 */ { 0, &Ia_cmovnaq_Gq_Eq },
  /* 0F 47 */ { 0, &Ia_cmovaq_Gq_Eq  },
  /* 0F 48 */ { 0, &Ia_cmovsq_Gq_Eq  },
  /* 0F 49 */ { 0, &Ia_cmovnsq_Gq_Eq },
  /* 0F 4A */ { 0, &Ia_cmovpq_Gq_Eq  },
  /* 0F 4B */ { 0, &Ia_cmovnpq_Gq_Eq },
  /* 0F 4C */ { 0, &Ia_cmovlq_Gq_Eq  },
  /* 0F 4D */ { 0, &Ia_cmovnlq_Gq_Eq },
  /* 0F 4E */ { 0, &Ia_cmovngq_Gq_Eq },
  /* 0F 4F */ { 0, &Ia_cmovgq_Gq_Eq  },
  /* 0F 50 */ { GRPSSE(0f50) },
  /* 0F 51 */ { GRPSSE(0f51) },
  /* 0F 52 */ { GRPSSE(0f52) },
  /* 0F 53 */ { GRPSSE(0f53) },
  /* 0F 54 */ { GRPSSE(0f54) },
  /* 0F 55 */ { GRPSSE(0f55) },
  /* 0F 56 */ { GRPSSE(0f56) },
  /* 0F 57 */ { GRPSSE(0f57) },
  /* 0F 58 */ { GRPSSE(0f58) },
  /* 0F 59 */ { GRPSSE(0f59) },
  /* 0F 5A */ { GRPSSE(0f5a) },
  /* 0F 5B */ { GRPSSE(0f5b) },
  /* 0F 5C */ { GRPSSE(0f5c) },
  /* 0F 5D */ { GRPSSE(0f5d) },
  /* 0F 5E */ { GRPSSE(0f5e) },
  /* 0F 5F */ { GRPSSE(0f5f) },
  /* 0F 60 */ { GRPSSE(0f60) },
  /* 0F 61 */ { GRPSSE(0f61) },
  /* 0F 62 */ { GRPSSE(0f62) },
  /* 0F 63 */ { GRPSSE(0f63) },
  /* 0F 64 */ { GRPSSE(0f64) },
  /* 0F 65 */ { GRPSSE(0f65) },
  /* 0F 66 */ { GRPSSE(0f66) },
  /* 0F 67 */ { GRPSSE(0f67) },
  /* 0F 68 */ { GRPSSE(0f68) },
  /* 0F 69 */ { GRPSSE(0f69) },
  /* 0F 6A */ { GRPSSE(0f6a) },
  /* 0F 6B */ { GRPSSE(0f6b) },
  /* 0F 6C */ { GRPSSE(0f6c) },
  /* 0F 6D */ { GRPSSE(0f6d) },
  /* 0F 6E */ { GRPSSE(0f6eQ) },
  /* 0F 6F */ { GRPSSE(0f6f) },
  /* 0F 70 */ { GRPSSE(0f70) },
  /* 0F 71 */ { GRPN(G12)    },
  /* 0F 72 */ { GRPN(G13)    },
  /* 0F 73 */ { GRPN(G14)    },
  /* 0F 74 */ { GRPSSE(0f74) },
  /* 0F 75 */ { GRPSSE(0f75) },
  /* 0F 76 */ { GRPSSE(0f76) },
  /* 0F 77 */ { 0, &Ia_emms  },
  /* 0F 78 */ { 0, &Ia_Invalid },
  /* 0F 79 */ { 0, &Ia_Invalid },
  /* 0F 7A */ { 0, &Ia_Invalid },
  /* 0F 7B */ { 0, &Ia_Invalid },
  /* 0F 7C */ { GRPSSE(0f7c) },
  /* 0F 7D */ { GRPSSE(0f7d) },
  /* 0F 7E */ { GRPSSE(0f7eQ) },
  /* 0F 7F */ { GRPSSE(0f7f) },
  /* 0F 80 */ { 0, &Ia_jo_Jd     },
  /* 0F 81 */ { 0, &Ia_jno_Jd    },
  /* 0F 82 */ { 0, &Ia_jb_Jd     },
  /* 0F 83 */ { 0, &Ia_jnb_Jd    },
  /* 0F 84 */ { 0, &Ia_jz_Jd     },
  /* 0F 85 */ { 0, &Ia_jnz_Jd    },
  /* 0F 86 */ { 0, &Ia_jbe_Jd    },
  /* 0F 87 */ { 0, &Ia_jnbe_Jd   },
  /* 0F 88 */ { 0, &Ia_js_Jd     },
  /* 0F 89 */ { 0, &Ia_jns_Jd    },
  /* 0F 8A */ { 0, &Ia_jp_Jd     },
  /* 0F 8B */ { 0, &Ia_jnp_Jd    },
  /* 0F 8C */ { 0, &Ia_jl_Jd     },
  /* 0F 8D */ { 0, &Ia_jnl_Jd    },
  /* 0F 8E */ { 0, &Ia_jle_Jd    },
  /* 0F 8F */ { 0, &Ia_jnle_Jd   },
  /* 0F 90 */ { 0, &Ia_seto_Eb   },
  /* 0F 91 */ { 0, &Ia_setno_Eb  },
  /* 0F 92 */ { 0, &Ia_setb_Eb   },
  /* 0F 93 */ { 0, &Ia_setnb_Eb  },
  /* 0F 94 */ { 0, &Ia_setz_Eb   },
  /* 0F 95 */ { 0, &Ia_setnz_Eb  },
  /* 0F 96 */ { 0, &Ia_setbe_Eb  },
  /* 0F 97 */ { 0, &Ia_setnbe_Eb },
  /* 0F 98 */ { 0, &Ia_sets_Eb   },
  /* 0F 99 */ { 0, &Ia_setns_Eb  },
  /* 0F 9A */ { 0, &Ia_setp_Eb   },
  /* 0F 9B */ { 0, &Ia_setnp_Eb  },
  /* 0F 9C */ { 0, &Ia_setl_Eb   },
  /* 0F 9D */ { 0, &Ia_setnl_Eb  },
  /* 0F 9E */ { 0, &Ia_setle_Eb  },
  /* 0F 9F */ { 0, &Ia_setnle_Eb },
  /* 0F A0 */ { 0, &Ia_pushq_FS  },
  /* 0F A1 */ { 0, &Ia_popq_FS   },
  /* 0F A2 */ { 0, &Ia_cpuid     },
  /* 0F A3 */ { 0, &Ia_btq_Eq_Gq },
  /* 0F A4 */ { 0, &Ia_shldq_Eq_Gq_Ib },
  /* 0F A5 */ { 0, &Ia_shldq_Eq_Gq_CL },
  /* 0F A6 */ { 0, &Ia_Invalid    },
  /* 0F A7 */ { 0, &Ia_Invalid    },
  /* 0F A8 */ { 0, &Ia_pushq_GS   },
  /* 0F A9 */ { 0, &Ia_popq_GS    },
  /* 0F AA */ { 0, &Ia_rsm        },
  /* 0F AB */ { 0, &Ia_btsq_Eq_Gq },
  /* 0F AC */ { 0, &Ia_shrdq_Eq_Gq_Ib },
  /* 0F AD */ { 0, &Ia_shrdq_Eq_Gq_CL },
  /* 0F AE */ { GRPN(G15)         },
  /* 0F AF */ { 0, &Ia_imulq_Gq_Eq    },
  /* 0F B0 */ { 0, &Ia_cmpxchgb_Eb_Gb },
  /* 0F B1 */ { 0, &Ia_cmpxchgq_Eq_Gq },
  /* 0F B2 */ { 0, &Ia_lssq_Gq_Mp   },
  /* 0F B3 */ { 0, &Ia_btrq_Eq_Gq   },
  /* 0F B4 */ { 0, &Ia_lfsq_Gq_Mp   },
  /* 0F B5 */ { 0, &Ia_lgsq_Gq_Mp   },
  /* 0F B6 */ { 0, &Ia_movzbq_Gq_Eb },
  /* 0F B7 */ { 0, &Ia_movzwq_Gq_Ew },
  /* 0F B8 */ { 0, &Ia_Invalid      },
  /* 0F B9 */ { 0, &Ia_ud2b         },
  /* 0F BA */ { GRPN(G8EqIb)        },
  /* 0F BB */ { 0, &Ia_btcq_Eq_Gq   },
  /* 0F BC */ { 0, &Ia_bsfq_Gq_Eq   },
  /* 0F BD */ { 0, &Ia_bsrq_Gq_Eq   },
  /* 0F BE */ { 0, &Ia_movsbq_Gq_Eb },
  /* 0F BF */ { 0, &Ia_movswq_Gq_Ew },
  /* 0F C0 */ { 0, &Ia_xaddb_Eb_Gb  },
  /* 0F C0 */ { 0, &Ia_xaddq_Eq_Gq  },
  /* 0F C2 */ { GRPSSE(0fc2)      },
  /* 0F C3 */ { GRPSSE(640fc3)    },
  /* 0F C4 */ { GRPSSE(0fc4)      },
  /* 0F C5 */ { GRPSSE(0fc5)      },
  /* 0F C6 */ { GRPSSE(0fc6)      },
  /* 0F C7 */ { GRPN(G9q)         },
  /* 0F C8 */ { 0, &Ia_bswapq_RRX },
  /* 0F C9 */ { 0, &Ia_bswapq_RRX },
  /* 0F CA */ { 0, &Ia_bswapq_RRX },
  /* 0F CB */ { 0, &Ia_bswapq_RRX },
  /* 0F CC */ { 0, &Ia_bswapq_RRX },
  /* 0F CD */ { 0, &Ia_bswapq_RRX },
  /* 0F CE */ { 0, &Ia_bswapq_RRX },
  /* 0F CF */ { 0, &Ia_bswapq_RRX },
  /* 0F D0 */ { GRPSSE(0fd0) },
  /* 0F D1 */ { GRPSSE(0fd1) },
  /* 0F D2 */ { GRPSSE(0fd2) },
  /* 0F D3 */ { GRPSSE(0fd3) },
  /* 0F D4 */ { GRPSSE(0fd4) },
  /* 0F D5 */ { GRPSSE(0fd5) },
  /* 0F D6 */ { GRPSSE(0fd6) },
  /* 0F D7 */ { GRPSSE(0fd7) },
  /* 0F D8 */ { GRPSSE(0fd8) },
  /* 0F D9 */ { GRPSSE(0fd9) },
  /* 0F DA */ { GRPSSE(0fda) },
  /* 0F DB */ { GRPSSE(0fdb) },
  /* 0F DC */ { GRPSSE(0fdc) },
  /* 0F DD */ { GRPSSE(0fdd) },
  /* 0F DE */ { GRPSSE(0fde) },
  /* 0F DF */ { GRPSSE(0fdf) },
  /* 0F E0 */ { GRPSSE(0fe0) },
  /* 0F E1 */ { GRPSSE(0fe1) },
  /* 0F E2 */ { GRPSSE(0fe2) },
  /* 0F E3 */ { GRPSSE(0fe3) },
  /* 0F E4 */ { GRPSSE(0fe4) },
  /* 0F E5 */ { GRPSSE(0fe5) },
  /* 0F E6 */ { GRPSSE(0fe6) },
  /* 0F E7 */ { GRPSSE(0fe7) },
  /* 0F E8 */ { GRPSSE(0fe8) },
  /* 0F E9 */ { GRPSSE(0fe9) },
  /* 0F EA */ { GRPSSE(0fea) },
  /* 0F EB */ { GRPSSE(0feb) },
  /* 0F EC */ { GRPSSE(0fec) },
  /* 0F ED */ { GRPSSE(0fed) },
  /* 0F EE */ { GRPSSE(0fee) },
  /* 0F EF */ { GRPSSE(0fef) },
  /* 0F F0 */ { GRPSSE(0ff0) },
  /* 0F F1 */ { GRPSSE(0ff1) },
  /* 0F F2 */ { GRPSSE(0ff2) },
  /* 0F F3 */ { GRPSSE(0ff3) },
  /* 0F F4 */ { GRPSSE(0ff4) },
  /* 0F F5 */ { GRPSSE(0ff5) },
  /* 0F F6 */ { GRPSSE(0ff6) },
  /* 0F F7 */ { GRPSSE(0ff7) },
  /* 0F F8 */ { GRPSSE(0ff8) },
  /* 0F F9 */ { GRPSSE(0ff9) },
  /* 0F FA */ { GRPSSE(0ffa) },
  /* 0F FB */ { GRPSSE(0ffb) },
  /* 0F FC */ { GRPSSE(0ffc) },
  /* 0F FD */ { GRPSSE(0ffd) },
  /* 0F FE */ { GRPSSE(0ffe) },
  /* 0F FF */ { 0, &Ia_Invalid }
};
