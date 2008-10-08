
// Init Hardware PCM decoder

			/*
			06a3 0082 0bb8 LRI	$R02, #0x0bb8
			06a5 195e      LRRI	$AC0.M, @$R02
			06a6 2ed1      SRS	@SampleFormat, $AC0.M
			06a7 195e      LRRI	$AC0.M, @$R02
			06a8 2ed4      SRS	@ACSAH, $AC0.M
			06a9 195e      LRRI	$AC0.M, @$R02
			06aa 2ed5      SRS	@ACSAL, $AC0.M
			06ab 195e      LRRI	$AC0.M, @$R02
			06ac 2ed6      SRS	@ACEAH, $AC0.M
			06ad 195e      LRRI	$AC0.M, @$R02
			06ae 2ed7      SRS	@ACEAL, $AC0.M
			06af 195e      LRRI	$AC0.M, @$R02
			06b0 2ed8      SRS	@ACCAH, $AC0.M
			06b1 195e      LRRI	$AC0.M, @$R02
			06b2 2ed9      SRS	@ACCAL, $AC0.M
			06b3 195e      LRRI	$AC0.M, @$R02
			06b4 2ea0      SRS	@COEF_A1_0, $AC0.M
			06b5 195e      LRRI	$AC0.M, @$R02
			06b6 2ea1      SRS	@COEF_A2_0, $AC0.M
			06b7 195e      LRRI	$AC0.M, @$R02
			06b8 2ea2      SRS	@COEF_A1_1, $AC0.M
			06b9 195e      LRRI	$AC0.M, @$R02
			06ba 2ea3      SRS	@COEF_A2_1, $AC0.M
			06bb 195e      LRRI	$AC0.M, @$R02
			06bc 2ea4      SRS	@COEF_A1_2, $AC0.M
			06bd 195e      LRRI	$AC0.M, @$R02
			06be 2ea5      SRS	@COEF_A2_2, $AC0.M
			06bf 195e      LRRI	$AC0.M, @$R02
			06c0 2ea6      SRS	@COEF_A1_3, $AC0.M
			06c1 195e      LRRI	$AC0.M, @$R02
			06c2 2ea7      SRS	@COEF_A2_3, $AC0.M
			06c3 195e      LRRI	$AC0.M, @$R02
			06c4 2ea8      SRS	@COEF_A1_4, $AC0.M
			06c5 195e      LRRI	$AC0.M, @$R02
			06c6 2ea9      SRS	@COEF_A2_4, $AC0.M
			06c7 195e      LRRI	$AC0.M, @$R02
			06c8 2eaa      SRS	@COEF_A1_5, $AC0.M
			06c9 195e      LRRI	$AC0.M, @$R02
			06ca 2eab      SRS	@COEF_A2_5, $AC0.M
			06cb 195e      LRRI	$AC0.M, @$R02
			06cc 2eac      SRS	@COEF_A1_6, $AC0.M
			06cd 195e      LRRI	$AC0.M, @$R02
			06ce 2ead      SRS	@COEF_A2_6, $AC0.M
			06cf 195e      LRRI	$AC0.M, @$R02
			06d0 2eae      SRS	@COEF_A1_7, $AC0.M
			06d1 195e      LRRI	$AC0.M, @$R02
			06d2 2eaf      SRS	@COEF_A2_7, $AC0.M
			06d3 195e      LRRI	$AC0.M, @$R02
			06d4 2ede      SRS	@GAIN, $AC0.M
			06d5 195e      LRRI	$AC0.M, @$R02
			06d6 2eda      SRS	@pred_scale, $AC0.M
			06d7 195e      LRRI	$AC0.M, @$R02
			06d8 2edb      SRS	@yn1, $AC0.M
			06d9 195e      LRRI	$AC0.M, @$R02
			06da 2edc      SRS	@yn2, $AC0.M
			*/

/// hmmmmmm
/*
	06db 8c00      CLR15	
	06dc 8a00      M2	
	06dd 8e00      S40	
*/

/// 

	AX0.L = *0xe16
	AX1.H = ratioHi     // sample ratio from AXPBSRC
	AX1.L = ratioLo     // sample ratio from AXPBSRC

	AC0 = 0
	AC0.L = currentAddressFrac	// AXPBSRC	
	
	*0x0e48 = last_samples[0]
	*0x0e49 = last_samples[1]
	*0x0e4A = last_samples[2]
	*0x0e4B = last_samples[3]
	
	AC1.M = AX1.L	
	ACC = ACC >> 0x05
	AC1 = AC1 + AC0
	
	R04 = AC1.M
	R05 = AC1.L
	
	AC1 = AC1 + 0xe0   // ?????? AC1 = AC1 - 2097152   (because 0xe0 is converted to signed and shift << 16)
	AC1 = AC1 >> 16
	AC1 = -AC1

	R06 = -AC1

//////////////
	AC1 = 0
	AC1.L = R05
	AC1 = AC1 << 2
	R05 = AC1.M


// 0x06fc

	AX.0 = 0x1fc
	AC0 = 0xe48
	R01 = 0xFFDD
	R03 = 0x0D80

// 0x0704
	for (i=0; i<R04; i++)
	{		
		AC0 = AC0 + AX1
		*R03++ = AC0.M

		AC1.M = AC0.L				
		LSR	$AC1.M, #0x79
		AC1 = AC1 & AX0.H
		AC1 += AX0.L		
		*R03++ = AC1

		*R00++ = *ADPCM_DECODER
		*R00++ = *ADPCM_DECODER
		*R00++ = *ADPCM_DECODER
		*R00++ = *ADPCM_DECODER
	}

/*
0704 0064 0715 BLOOP	$R04, 0x0715
0706 1827      LRR	$R07, @$R01
0707 1b07      SRRI	@$R00, $R07
0708 4a00      ADDAX	$AC0.M, $AX1.L
0709 1ffc      MRR	$AC1.M, $AC0.L
070a 1827      LRR	$R07, @$R01
070b 1b07      SRRI	@$R00, $R07
070c 1579      LSR	$AC1.M, #0x79
070d 3500      ANDR	$AC1.M, $R00
070e 1827      LRR	$R07, @$R01
070f 1b07      SRRI	@$R00, $R07
0710 4100      ADDR	$AC1.M, $AX0.L
0711 1b7e      SRRI	@$R03, $AC0.M
0712 1827      LRR	$R07, @$R01
0713 1b07      SRRI	@$R00, $R07
0714 1b7f      SRRI	@$R03, $AC1.M
0715 0000      NOP */	

// 0x0715 
// prolly copies the "rest" 

	for (i=0; i<r05; i++)
	{
		R07 = *ADPCM_DECODER
		*R00++ = R07
	}

	// 0x71c
	R03--
	AC1 = *R03

/* 071c 0007      DAR	$R03
071d 187f      LRR	$AC1.M, @$R03 */

	for (i<0; i<r06; i++)
	{
		AC0 = AX1
		*R03++ = AC1.M
		
	}

/*
071e 0066 0724 BLOOP	$R06, 0x0724
0720 4a3b      ADDAXÌS	$AC0.M, $AX1.L : @$R03, $AC1.M
0721 1ffc      MRR	$AC1.M, $AC0.L
0722 1579      LSR	$AC1.M, #0x79
0723 3533      ANDRÌS	$AC1.M, $R00 : @$R03, $AC0.M
0724 4100      ADDR	$AC1.M, $AX0.L
*/

0725 1b7f      SRRI	@$R03, $AC1.M
0726 0004      DAR	$R00
0727 189f      LRRD	$AC1.M, @$R00
0728 1adf      SRRD	@$R02, $AC1.M
0729 189f      LRRD	$AC1.M, @$R00
072a 1adf      SRRD	@$R02, $AC1.M
072b 189f      LRRD	$AC1.M, @$R00
072c 1adf      SRRD	@$R02, $AC1.M
072d 189f      LRRD	$AC1.M, @$R00
072e 1adf      SRRD	@$R02, $AC1.M
072f 1adc      SRRD	@$R02, $AC0.L
0730 0082 0bd2 LRI	$R02, #0x0bd2
0732 27dc      LRS	$AC1.M, @yn2
0733 1adf      SRRD	@$R02, $AC1.M
0734 27db      LRS	$AC1.M, @yn1
0735 1adf      SRRD	@$R02, $AC1.M
0736 27da      LRS	$AC1.M, @pred_scale
0737 1adf      SRRD	@$R02, $AC1.M
0738 0082 0bbe LRI	$R02, #0x0bbe
073a 27d9      LRS	$AC1.M, @ACCAL
073b 1adf      SRRD	@$R02, $AC1.M
073c 27d8      LRS	$AC1.M, @ACCAH
073d 1adf      SRRD	@$R02, $AC1.M


073e 8f00      S16	
073f 00c1 0e42 LR	$R01, @0x0e42
0741 0082 0d80 LRI	$R02, #0x0d80
0743 1940      LRRI	$R00, @$R02
0744 1943      LRRI	$R03, @$R02
0745 80f0      NXÌLDX	: $AX1.L, $AX1.H, @$R01
0746 b8c0      MULXÌLDX	$AX0.H, $AX1.H : $AX0.L, $AX0.H, @$R00
0747 111f 074f BLOOPI	#0x1f, 0x074f
0749 a6f0      MULXMVÌLDX	$AX0.L, $AX1.L, $AC0.M : $AX1.L, $AX1.H, @$R01
074a bcf0      MULXACÌLDX	$AX0.H, $AX1.H, $AC0.M : $AX1.L, $AX1.H, @$R01
074b 1940      LRRI	$R00, @$R02
074c 1943      LRRI	$R03, @$R02
074d bcf0      MULXACÌLDX	$AX0.H, $AX1.H, $AC0.M : $AX1.L, $AX1.H, @$R01
074e 4ec0      ADDPÌLDX	$AC0.M : $AX0.L, $AX0.H, @$R00
074f b831      MULXÌS	$AX0.H, $AX1.H : @$R01, $AC0.M
0750 a6f0      MULXMVÌLDX	$AX0.L, $AX1.L, $AC0.M : $AX1.L, $AX1.H, @$R01
0751 bcf0      MULXACÌLDX	$AX0.H, $AX1.H, $AC0.M : $AX1.L, $AX1.H, @$R01
0752 bc00      MULXAC	$AX0.H, $AX1.H, $AC0.M
0753 4e00      ADDP	$AC0.M
0754 1b3e      SRRI	@$R01, $AC0.M
0755 00e1 0e42 SR	@0x0e42, $R01
0757 02df      RET	

