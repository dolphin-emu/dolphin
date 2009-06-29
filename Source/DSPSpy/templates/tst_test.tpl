<test name="acc-16 test" type="srfull"
	  description="This test checks the effect of various ACC values over tst on acc0">
<header>
incdir  "tests"
include "dsp_base.inc"
</header>
<body>

lri $SR,  #0x0000
clr $acc0
lri $ac0.@CMD@ @SR@
tst $acc0

call send_back  

</body>
</test>