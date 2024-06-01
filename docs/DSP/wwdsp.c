/* important variables */
short mem[0x1000];
#define v_0341 mem[0x0341] /* op#2 saves a copy of 344 here */
#define v_0343 mem[0x0343] /* message type? */
#define v_0344 mem[0x0344] /* low byte of first word of message */
#define v_0345 mem[0x0345] /* copy of second word of message */
#define a_0346 mem[0x0346] /* buffer for message contents, size unknown */
#define v_034e mem[0x034e] /* init to 0, later set to most recent value written in a_04fc */

#define v_0350 mem[0x0350] /* init to 0x0280 (write location of messages) */
#define v_0351 mem[0x0351] /* init to 0x0280 (read location?) */
#define v_0352 mem[0x0352] /* init to 0 (number of messages pending?) */
#define v_0355 mem[0x0355] /* used by op#2, init to 0 */
#define a_0356 (mem+0x0356) /* message contents */

#define a_0388 (mem+0x388) /* where op 2 keeps its message */
#define a_038e (mem+0x38e) /* op#2 saves a copy of 345 here */

#define a_03a8 (mem+0x3a8) /* where op#2 dmas in its data, at least 0x80 */

#define v_03fa mem[0x03fa] /* temp for ax0.h during exception */
#define v_03fb mem[0x03fb] /* temp for ar0 during exception */
#define v_03fc mem[0x03fc] /* temp for r08 during exception */
#define v_03fd mem[0x03fd] /* temp for ac0.h during exception */
#define v_03fe mem[0x03fe] /* temp for ac0.m during exception */
#define v_03ff mem[0x03ff] /* temp for ac0.l during exception */

#define a_04e8 (mem+0x04e8) /* 4 element array */
#define a_04ec (mem+0x04ec) /* 4 element array (empty) */
#define a_04f0 (mem+0x04f0) /* 4 element array */
#define a_04f4 (mem+0x04f4) /* 4 element array (empty) */
#define a_04fc (mem+0x04fc) /* 16 element array, written by messages */
#define a_09a0 (mem+0x09a0) /* 0x50 element array, used by op#2, cleared */
#define a_0a00 (mem+0x0a00) /* 0x50 element array, used by op#2, cleared */
#define a_0b00 (mem+0x0b00) /* array from 0x0b00-0x0bff, involves accelerator? */
#define a_0ca0 (mem+0x0ca0) /* 0x50 element array, used by op#2, cleared */
#define a_0d00 (mem+0x0d00) /* 0x50 element array, used by op#2, cleared */
#define a_0d60 (mem+0x0d60) /* 0x50 element array, used by op#2, cleared */
#define a_0f40 (mem+0x0f40) /* 0x50 element array, used by op#2, cleared */



/* reset exception vector 0x0000 */
void main() {
	/* JMP 0x0012 */
	/* various inits */
	SBCLR(5); /* disable interrupts? */
	fcn_0057(); /* init hardware stuff */
	for(ac1.m=0x1000,ac0.m=0;ac1.m>0;ac1.m--) mem[ac0.m++]=0; /* clear all vars */
	fcn_0688(); /* init some vars */
	fcn_04c0(); /* set something up related to the accelerator at a_0b00 */
	fcn_0e14(); /* set up a table */
	fcn_066a(0); /* send a message */
	fcn_0674(0x1111); /* send a message */
	v_034e=0;
	SBSET(5); /* enable interrupts? */

	/* jump to 0x06c5 */
mainloop:
	while (v_0352) ; /* while no messages pending */
		
	SBCLR(5); /* do not distrub */
	v_0352--; /* important that this be atomic */
	SBSET(5);
	
	t=v_0351;
	size=mem[t++];
	if (!(size&0x8000)) { /* size > 0x7fff invalid */
		if (size==0) { /* die on message of length 0? */
			/* jump to 0x06f5 */
			/* jump to 0x05f0 */
			
			/* TODO: analysis of HALT */
			HALT();
		}
		for (i=size,j=0;i>0;i--) {
			a_0356[j++]=mem[t++]; 
			a_0356[j++]=mem[t++];
		}
		v_0351=t;
		
		/* jump to 0x002f */
		/* actual command handling */
		v_0345=a_0356[1];
		v_0344=a_0356[0]&0x00ff;
		v_0343=(a_0346[0]>>7)&0x7e;
		
		/* jump table at 0x75 used here */
		switch (v_0343>>1) {

			// 0x43
			case 0:
			case 10:
			case 11:
			case 12:
			case 14:
			case 15:
				/* invalid command? */
				config=0x00ff;
				fcn_066a(0x04); /* send off a message */
				fcn_0674(a_0356[0]); /* send first word of command as a message */
				goto mainloop;
				break;
			case 1:
				/* jmp 0x0095 */
				break;

			case 2:
				/* jmp 0x0243 */
				sub_0243();
				break;

			case 3:
				/* jmp 0x0073 */
				break;

			case 4:
				/* jmp 0x580 */
				break;

			case 5:
				/* jmp 0x592 */
				break;

			case 6:
				/* jmp 0x469 */
				break;

			case 7:
				/* jmp 0x41d */
				break;

			case 8: /* mix */
				/* jmp 0x0485 */
				fcn_0067(fcn_0067(0x0346)); /* read message twice? */
				/* take in the two buffers to mix */
				fcn_0525(mem[0x344],mem[0x346],mem[0x347],0x400); /* size, addrH, addrL, dsp addr */
				fcn_0525(mem[0x344],mem[0x348],mem[0x349],0x800);
				S16(); /* saturate all adds, multiplies to 16 bits? */
				
				i=mem[0x0344];
				src1=0x400;
				src2=0x800;
				scale=mem[0x345];
								
				prod=scale*mem[src2++];
				val2=mem[src2++];
				do {
					val1=mem[src1];
					val1+=prod;
					prod=scale*val2;
					mem[src1]=val1;
					val2=mem[src2];
					src1++;
					src2++;
				} while (--i);
				
				S40();
				
				/* dma out mixed buf */
				fcn_0523(mem[0x344],mem[0x346],mem[0x347],0x400);
				
				break;

			case 9:
				/* jmp 0x44d */
				break;

			case 13:
				/* jmp 0x00b2 */
				break;
		}
	}
	
	v_0351=t;
	
	goto mainloop;
}

/* message in MBOX exception vector? 0x000e */
void exception() {
	/* JMP 0x05b8 */
	
	SBCLR(5);
	S40();
	/* save ax0.h,ac0.h,ac0.m, and ac0.l */
	
	if ((tH=register_fffe)&0x8000) { /* CPU mailbox H */
		if (!(tsize=register_ffff)) { /* CPU mailbox L */
			/* empty message? */
			while (!((tH=register_fffe)&0x8000)) ;
			tH&=0xf;
			v_034e=(tH+1)<<4;
			a_04fc[tH]=register_ffff;
		} else { /* nonempty message? */
			/* jump to 0x0692 */
			/* save ar0, r08 */
			t=v_0350;
			mem[t++]=tsize;
			
			do {
				while (!((tH=register_fffe)&0x8000)) ;
				mem[t++]=tH;
				mem[t++]=register_ffff;
			} while (--tsize);
			
			v_0350=t;
			v_0352++;
			/* restore ar0, r08 */
			
			/* jump to 0x05e6 */
		}
	} else { /* interrupt without message? */
		/* jump to 0x06b9 */
		/* save ar0, r08 */
		mem[v_0350]=0; /* empty message */
		/* jump to 0x06ab */
		v_0350++;
		v_0352++;
		/* restore ar0, r08 */
		
		/* jump to 0x05e6 */
	}
	
	/* 0x05e6 */
	
	/* restore ax0.h,ac0.h,ac0.m, and ac0.l */
	SBSET(5);
	/* RTI */
}

/* set up some registers */
void fcn_0057() {
	SBCLR(2);
	SBCLR(3);
	SBCLR(4);
	SBCLR(6);
	S40(); /* 40-bit mode */
	CLR15();
	M0(); /* don't multiply result by 2 */
	r08=-1;
	r09=-1;
	r0a=-1;
	r0b=-1;
	config=0xff;
}

void fcn_0688() {
	v_0350=0x0280;
	v_0351=0x0280;
	v_0352=0;
}

void fcn_04c0() {
	config=0xff;
	for(i=0xff,ar0=0xb00;i>0;i--) mem[ar0++]=0;
	mem[ar0++]=0; /* get the last one */
	fcn_0573(0x0b00,0x0100,0);
}

/* a=an address in ac1.m, l=a length in ar0, v=a value? in ac0.m */
void fcn_0573(short a, short l, short v) {
	fcn_0561(a,l,0x0001);
}

/* a=an address in ac1.m, l=a length in ar0, v=a value? in ac0.m, f is a flag? in ac0.h */
/* return is in ax0.h */
short fcn_0561(short a, short l, short v, short f) {
	register_ffd1=0x0a; /* unknown reg, accel? */
	register_ffd6=-1; /* accel end addr H */
	register_ffd7=-1; /* accel end addr L */
	register_ffd8=v>>1; /* 
	register_ffd9=?; /* has a value from way back? */
	
	return f;
}

/* initializes some tables that look useful... */
void fcn_0e14() {
	a_04e8[0]=0x8240;
	a_04e8[1]=0x7fff;
	a_04e8[2]=0x7dbf;
	a_04e8[3]=0x843f;
	a_04f0[0]=0xb23b;
	a_04f0[1]=0x7fff;
	a_04f0[2]=0x4dc4;
	a_04f0[3]=0xd808;
	a_04ec[0]=a_04ec[1]=a_04ec[2]=a_04ec[3]=0;
	a_04f4[0]=a_04f4[1]=a_04f4[2]=a_04f4[3]=0;
}

/* send a message via DSP MBOX */
void fcn_066a(short m) {
	fcn_0682(); /* wait for empty mbox */
	register_fffc=0xdcd1;
	register_fffd=m;
	register_fffb=1; /* IRQ */
	fcn_0682();
}

/* wait for dsp mbox empty */
void fcn_0682() {
	while (register_fffc&0x8000);
}

void fcn_0674(short m) {
	fcn_0682();
	register_fffc=0xf355;
	register_fffd=m;
	fcn_0682();
}

/* a=address in ar0 */
/* fetch a message body (up to zero)? */
short fcn_0067(short a) {
	i=0x0357;
	j=a;
	do {
		mem[j++]=mem[i++];
		mem[j++]=mem[i];
	} while (mem[i++]);
	return a;
}

/* dma in, I assume */
/* size=words to transfer in ar0, addrL=low word of RAM address in ac0.l, addrH=high word of RAM address in ac0.m, dspaddr=dsp address in ac1.m */
void fcn_0525(short size, short addrH, short addrL, short dspaddr) {
	register_ffcd=dspaddr; /* dsp address */
	register_ffc9=0; /* direction: ram->dsp */
	register_ffce=addrH; /* memory address H */
	register_ffcf=addrL; /* memory address L */
	register_ffcb=size<<1; /* bytes to transfer (size must be in words) */
	fcn_0536(); /* dma wait */
}

/* dma wait? */
void fcn_0536() {
	while (!(register_ffc9&4));
}

/* dma out, I assume */
/* size=words to transfer in ar0, addrL=low word of RAM address is ac0.l, addrH=high word of RAM address in ac0.m, dspaddr=dsp address in ac1.m */
/* shares code with fcn_0525 */
void fcn_0523(short size, short addrH, short addrL, shot dspaddr) {
	register_ffcd=dspaddr;
	/* jump back into 0525 */
	register_ffc9=1; /* direction dsp->ram */
	register_ffce=addrH;
	register_ffcf=addrL;
	register_ffcb=size<<1;
	fcn_0536();
}

/* huge subroutine, op #2 */
void sub_0243() {
	fcn_0067(0x0388); /* called in an indirect manner... */
	v_0341=v_0344; /* low byte first word of message */
	v_038e=v_0345;
	v_0355=0;
	fcn_022a(); /* get stuffs */
	fcn_05a4(); /* write to accel */
	
	for (i=v_0341;i>0i--) {
		fcn_0102();
		
	}
}

void fcn_022a() {
	/* something else must set 386, 387 */
	fcn_0525(0x0040,v_0386,v_0387,0x03a8);
}

void fcn_05a4() {
	register_ffd4=-1;
	register_ffd5=-1;
	register_ffd6=-1;
	register_ffd7=-1;
}

void fcn_0102() {
	for (i=0;i<0x50;i++) a_0d00[i]=0;
	for (i=0;i<0x50;i++) a_0d60[i]=0;
	fcn_0e3f();
	for (i=0;i<0x50;i++) a_0ca0[i]=0;
	for (i=0;i<0x50;i++) a_0f40[i]=0;
	for (i=0;i<0x50;i++) a_0fa0[i]=0;
	for (i=0;i<0x50;i++) a_0a00[i]=0;
	for (i=0;i<0x50;i++) a_09a0[i]=0;
}

void fcn_0e3f() {
	fcn_00fa(0x0f40,0x0b00,0x50,0x6784);
	fcn_0ba4(0x04e8,0x0b00,0x04ec);
}

/* addr1=address in ar0, addr2=address in ar3, size=size of table at addr1 in ac1.m, val=in ax0.l */
void fcn_00fa(short addr1, short addr2, short size, short val) {
	M2(); /* all multiplications 2x */
		
	tmp=mem[addr1++];
	prod=val*tmp*2;
	tmp=mem[addr1++];
	ac0.m=prod;
	prod=val*tmp*2;
	tmp=mem[addr1++];
	
	do {
		ac0.m=prod;
		prod=val*tmp*2;
		mem[addr2]=ac0.m;
		tmp=mem[addr1];
		addr1++;
		addr2++;
	} while (--size);
	
	M0();
}

/* addr1=address in ar0 (source 4 element table?), addr2=address in ar1 (accelerator?), addr3=address in ar2 (destination 4 element table?) */
void fcn_00ba4(short addr1, short addr2, short addr3) {
	
}