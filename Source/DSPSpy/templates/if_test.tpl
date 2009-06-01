<test name="@CMD@ test" type="srfull"
	  description="This test checks the effect of various SR flags over @CMD@">
<header>
incdir  "tests"
include "dsp_base.inc"
</header>
<body>
lri $IX0, #0x0000
lri $SR, @SR@

@CMD@
lri $IX0, #0x1337       
call send_back  
</body>
</test>