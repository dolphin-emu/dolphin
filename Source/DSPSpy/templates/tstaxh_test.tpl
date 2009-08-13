<test name="tstaxh test" type="srfull"
	  description="This test checks the effect of various ax0.h values over tstaxh">
<header>
incdir  "tests"
include "dsp_base.inc"
</header>
<body>

lri $SR,  #0x0000
lri $ax0.h, @SR@
tstaxh $ax0.h

call send_back  

lri $SR,  #0xffff
lri $ax0.h, @SR@
tstaxh $ax0.h

call send_back  

</body>
</test>