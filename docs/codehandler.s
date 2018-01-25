#MIT License

#Copyright (c) 2010-2017 Nuke, brkirch, Y.S, Kenobi, gamemasterplc

#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:

#The above copyright notice and this permission notice shall be included in all
#copies or substantial portions of the Software.

#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#SOFTWARE.

#Based off of codehandleronly.s from Gecko OS source code.

.text
#Register Defines

.set r0,0;   .set r1,1;   .set r2,2; .set r3,3;   .set r4,4
.set r5,5;   .set r6,6;   .set r7,7;   .set r8,8;   .set r9,9
.set r10,10; .set r11,11; .set r12,12; .set r13,13; .set r14,14
.set r15,15; .set r16,16; .set r17,17; .set r18,18; .set r19,19
.set r20,20; .set r21,21; .set r22,22; .set r23,23; .set r24,24
.set r25,25; .set r26,26; .set r27,27; .set r28,28; .set r29,29
.set r30,30; .set r31,31; .set f0,0; .set f2,2; .set f3,3
         
.globl _start

gameid:
.long	0,0
cheatdata:
.long	frozenvalue
.space 39*4

_start:
	stwu	r1,-168(r1)		# stores sp
	stw	r0,8(r1)		# stores r0

	mflr	r0
	stw	r0,172(r1)		# stores lr

	mfcr	r0
	stw	r0,12(r1)		# stores cr

	mfctr	r0
	stw	r0,16(r1)		# stores ctr

	mfxer	r0
	stw	r0,20(r1)		# stores xer

	stmw	r3,24(r1)		# saves r3-r31

	mfmsr	r25
	ori	r26,r25,0x2000		#enable floating point ?
	andi.	r26,r26,0xF9FF
	mtmsr	r26


	stfd	f2,152(r1)		# stores f2
	stfd	f3,160(r1)		# stores f3



    lis	r31,_start@h		#0x8000
    
    lis r20, 0xCC00
    lhz r28, 0x4010(r20)
    ori r21, r28, 0xFF
    sth r21, 0x4010(r20) # disable MP3 memory protection

	mflr	r29
	lis r15, codelist@h
	ori	r15, r15, codelist@l

	ori	r7, 31, cheatdata@l	# set pointer for storing data (before the codelist)

	lis	r6,0x8000		# default base address = 0x80000000 (code handler)

	mr	r16,r6			# default pointer =0x80000000 (code handler)

	li	r8,0			# code execution status set to true (code handler)

	lis	r3,0x00D0
    ori	r3,r3,0xC0DE

	lwz	r4,0(r15)
	cmpw	r3,r4
	bne-	_exitcodehandler
	lwz	r4,4(r15)
	cmpw	r3,r4
    bne-	_exitcodehandler	# lf no code list skip code handler
    addi	r15,r15,8
	b	_readcodes
_exitcodehandler:
	mtlr	r29
resumegame:

    sth r28,0x4010(r20) # restore memory protection value

	lfd	f2,152(r1)		# loads f2
	lfd	f3,160(r1)		# loads f3

	mfmsr	r25

	lwz	r0,172(r1)
	mtlr	r0			# restores lr

	lwz	r0,12(r1)
	mtcr	r0			# restores cr

	lwz	r0,16(r1)
	mtctr	r0			# restores ctr

	lwz	r0,20(r1)
	mtxer	r0			# restores xer

	lmw	r3,24(r1)		# restores r3-r31

	lwz	r0,8(r1)		# loads r0

	addi	r1,r1,168

	isync

    blr				# return back to game

_readcodes:

	lwz	r3,0(r15)		#load code address
	lwz	r4,4(r15)		#load code value

	addi	r15,r15,8		#r15 points to next code

	andi.	r9,r8,1
	cmpwi	cr7,r9,0		#check code execution status in cr7. eq = true, ne = false

	li	r9,0			#Clears r9

	rlwinm	r10,r3,3,29,31		#r10 = extract code type, 3 bits
	rlwinm 	r5,r3,7,29,31		#r5  = extract sub code type 3 bits

	andis.	r11,r3,0x1000		#test pointer
	rlwinm	r3,r3,0,7,31		#r3  = extract address in r3 (code type 0/1/2) #0x01FFFFFF

	bne	+12			#jump lf the pointer is used

	rlwinm	r12,r6,0,0,6		#lf pointer is not used, address = base address
	b	+8

	mr	r12,r16			#lf pointer is used, address = pointer


	cmpwi	cr4,r5,0		#compares sub code type with 0 in cr4

	cmpwi	r10,1
	blt+	_write			#code type 0 : write
	beq+	_conditional		#code type 1 : conditional

	cmpwi	r10,3
	blt+	_ba_pointer		#Code type 2 : base address operation

	beq-	_repeat_goto		#Code type 3 : Repeat & goto

	cmpwi	r10,5
	blt-	_operation_rN		#Code type 4 : rN Operation
	beq+	_compare16_NM_counter	#Code type 5 : compare [rN] with [rM]

	cmpwi	r10,7
	blt+	_hook_execute		#Code type 6 : hook, execute code
	
	b	_terminator_onoff_	#code type 7 : End of code list

#CT0=============================================================================
#write  8bits (0): 00XXXXXX YYYY00ZZ
#write 16bits (1): 02XXXXXX YYYYZZZZ
#write 32bits (2): 04XXXXXX ZZZZZZZZ
#string code  (3): 06XXXXXX YYYYYYYY, d1d1d1d1 d2d2d2d2, d3d3d3d3 ....
#Serial Code  (4): 08XXXXXX YYYYYYYY TNNNZZZZ VVVVVVVV

_write:
	add	r12,r12,r3		#address = (ba/po)+(XXXXXX)
	cmpwi	r5,3
	beq-	_write_string		#r5  == 3, goto string code
	bgt-	_write_serial		#r5  >= 4, goto serial code

	bne-	cr7,_readcodes		#lf code execution set to false skip code

	cmpwi	cr4,r5,1		#compares sub code type and 1 in cr4

	bgt-	cr4,_write32		#lf sub code type == 2, goto write32

#lf sub code type = 0 or 1 (8/16bits)
	rlwinm	r10,r4,16,16,31		#r10 = extract number of times to write (16bits value)

_write816:
	beq	cr4,+32			#lf r5 = 1 then 16 bits write
	stbx	r4,r9,r12		#write byte
	add r21, r9, r12
	icbi r0, r21
	sync
	isync
	addi	r9,r9,1
	b	+28
	sthx	r4,r9,r12		#write halfword
	add r21, r9, r12 #Get Real Memory Offset
	icbi r0, r21 #Invalidate Icache around real memory offset
	sync
	isync
	addi	r9,r9,2
	subic.	r10,r10,1		#number of times to write -1
	bge-	_write816
	b	_readcodes

_write32:
	rlwinm	r12,r12,0,0,29		#32bits align adress
    stw	r4,0(r12)		#write word to address
	icbi r0, r12 #Invalidate icache around address
	sync
	isync
    b	_readcodes

_write_string:				#endianess ?
	mr	r9,r4
	bne-	cr7,_skip_and_align	#lf code execution is false, skip string code data


	_stb:
	subic.	r9,r9,1			#r9 -= 1 (and compares r9 with 0)
	blt-	_skip_and_align		#lf r9 < 0 then exit
	lbzx	r5,r9,r15
	stbx	r5,r9,r12		#loop until all the data has been written
	add r21, r9, r12 #Get Real Memory Offset
	icbi r0, r21 #Invalidate Icache around real memory offset
	sync
	isync
	b	_stb

_write_serial:

	addi	r15,r15,8		#r15 points to the code after the serial code
	bne-	cr7,_readcodes		#lf code execution is false, skip serial code

	lwz	r5,-8(r15)		#load TNNNZZZZ
	lwz	r11,-4(r15)		#r11 = load VVVVVVVV

	rlwinm	r17,r5,0,16,31		#r17 = ZZZZ
	rlwinm	r10,r5,16,20,31		#r10 = NNN (# of times to write -1)
	rlwinm	r5,r5,4,28,31		#r5  = T (0:8bits/1:16bits/2:32bits)


_loop_serial:
	cmpwi	cr5,r5,1
	beq-	cr5,+16			#lf 16bits
	bgt+	cr5,+20			#lf 32bits
	
	stbx	r4,r9,r12		#write serial byte (CT04,T=0)
	b	+16

	sthx	r4,r9,r12		#write serial halfword (CT04,T=1)
	b	+8

	stwx	r4,r9,r12		#write serial word (CT04,T>=2)
	add r21, r9, r12 #Get Real Memory Offset
	icbi r0, r21 #Invalidate Icache around real memory offset
	sync
	isync
	add	r4,r4,r11		#value +=VVVVVVVV
	add	r9,r9,r17		#address +=ZZZZ
	subic.	r10,r10,1
	bge+	_loop_serial		#loop until all the data has been written

	b	_readcodes

#CT1=============================================================================
#32bits conditional (0,1,2,3): 20XXXXXX YYYYYYYY
#16bits conditional (4,5,6,7): 28XXXXXX ZZZZYYYY

#PS : 31 bit of address = endlf.

_conditional:
	rlwinm.	r9,r3,0,31,31		#r10 = (bit31 & 1) (endlf enabled?)

	beq	+16			#jump lf endlf is not enabled

	rlwinm	r8,r8,31,1,31		#Endlf (r8>>1)
	andi.	r9,r8,1			#r9=code execution status
	cmpwi	cr7,r9,0		#check code execution status in cr7
	cmpwi	cr5,r5,4		#compares sub code type and 4 in cr5
	cmpwi	cr3,r10,5		#compares code type and 5 in cr3

	rlwimi	r8,r8,1,0,30		#r8<<1 and current execution status = old execution status
	bne-	cr7,_true_end		#lf code execution is set to false -> exit

	bgt	cr3,_addresscheck2	#lf code type==6 -> address check
	add	r12,r12,r3		#address = (ba/po)+(XXXXXX)



	blt	cr3,+12			#jump lf code type <5 (==1)
	blt	cr5,_condition_sub	#compare [rN][rM]
	b	_conditional16_2	#counter compare
	bge	cr5,_conditional16	#lf sub code type>=4 -> 16 bits conditional

_conditional32:
	rlwinm	r12,r12,0,0,29		#32bits align
	lwz	r11,0(r12)
	b	_condition_sub

_conditional16:
	rlwinm	r12,r12,0,0,30		#16bits align
	lhz	r11,0(r12)
_conditional16_2:
	nor	r9,r4,r4
	rlwinm	r9,r9,16,16,31		#r9  = extract mask
	and	r11,r11,r9		#r11 &= r9
	rlwinm	r4,r4,0,16,31		#r4  = extract data to check against

_condition_sub:
	cmpl	cr6,r11,r4		#Unsigned compare. r11=data at address, r4=YYYYYYYY
	andi.	r9,r5,3
	beq	_skip_NE		#lf sub code (type & 3) == 0
	cmpwi	r9,2
	beq	_skip_LE		#lf sub code (type & 3) == 2
	bgt	_skip_GE		#lf sub code (type & 3) == 3

_skip_EQ:#1
	bne-	cr6,_true_end		#CT21, CT25, CT29 or CT2D (lf !=)
	b	_skip

_skip_NE:#0
	beq-	cr6,_true_end		#CT20, CT24, CT28 or CT2C (lf==)
	b	_skip

_skip_LE:#2
	bgt-	cr6,_true_end		#CT22, CT26, CT2A or CT2E (lf r4>[])
	b	_skip

_skip_GE:#3
	blt-	cr6,_true_end		#CT23, CT27, CT2B or CT2F (lf r4<r4)


_skip:
	ori	r8,r8,1			#r8|=1 (execution status set to false)
_true_end:
	bne+	cr3,_readcodes		#lf code type <> 5
	blt	cr5,_readcodes
	lwz	r11,-8(r15)		#load counter
	bne	cr7,_clearcounter	#lf previous code execution false clear counter
	andi.	r12,r3,0x8		#else lf clear counter bit not set increase counter
	beq	_increase_counter
	andi.	r12,r8,0x1		#else lf.. code result true clear counter
	beq	_clearcounter


_increase_counter:
	addi	r12,r11,0x10		#else increase the counter
	rlwimi	r11,r12,0,12,27		#update counter
	b	_savecounter

_clearcounter:
	rlwinm	r11,r11,0,28,11		#clear the counter
_savecounter:
	stw	r11,-8(r15)		#save counter
	b _readcodes


#CT2============================================================================

#load base adress    (0): 40TYZ00N XXXXXXXX = (load/add:T) ba from [(ba/po:Y)+XXXXXXXX(+rN:Z)]

#set base address    (1): 42TYZ00N XXXXXXXX = (set/add:T) ba to (ba/po:Y)+XXXXXXXX(+rN:Z)

#store base address  (2): 440Y0000 XXXXXXXX = store base address to [(ba/po)+XXXXXXXX]
#set base address to (3): 4600XXXX 00000000 = set base address to code address+XXXXXXXX
#load pointer        (4): 48TYZ00N XXXXXXXX = (load/add:T) po from [(ba/po:Y)+XXXXXXXX(+rN:Z)]

#set pointer         (5): 4ATYZ00N XXXXXXXX = (set/add:T) po to (ba/po:Y)+XXXXXXXX(+rN:Y)

#store pointer       (6): 4C0Y0000 XXXXXXXX = store pointer to [(ba/po)+XXXXXXXX]
#set pointer to      (7): 4E00XXXX 00000000 = set pointer to code address+XXXXXXXX

_ba_pointer:

	bne-	cr7,_readcodes


	rlwinm	r9,r3,2,26,29		#r9  = extract N, makes N*4

	rlwinm	r14,r3,16,31,31		#r3 = add ba/po flag bit (Y)
	cmpwi	cr3,r14,0

	cmpwi	cr4,r5,4		#cr4 = compare sub code type with 4 (ba/po)
	andi.	r14,r5,3		#r14 = sub code type and 3
	
	cmpwi	cr5,r14,2		#compares sub code type and 2

	blt-	cr5,_p01
	beq-	cr5,_p2			#sub code type 2

_p3:
	extsh	r4,r3
	add	r4,r4,r15		#r4=XXXXXXXX+r15 (code location in memory)
	b	_pend

_p01:

	rlwinm.	r5,r3,20,31,31		#r3 = rN use bit (Z)
	beq	+12			#flag is not set(=0), address = XXXXXXXX

	lwzx	r9,r7,r9		#r9 = load register N
	add	r4,r4,r9		#flag is set (=1), address = XXXXXXXX+rN

	beq	cr3,+8			#(Y) flag is not set(=0), address = XXXXXXXX (+rN)
 
  	add	r4,r12,r4		#address = XXXXXXXX (+rN) + (ba/po)


	cmpwi	cr5,r14,1			
	beq	cr5,+8			#address = (ba/po)+XXXXXXXX(+rN)
	lwz	r4,0(r4)		#address = [(ba/po)+XXXXXXXX(+rN)]

	rlwinm.	r3,r3,12,31,31		#r5 = add/replace flag (T)
	beq	_pend			#flag is not set (=0), (ba/po)= XXXXXXXX (+rN) + (ba/po)
	bge	cr4,+12
	add	r4,r4,r6		#ba += XXXXXXXX (+rN) + (ba/po)
	b	_pend
	add	r4,r4,r16		#po += XXXXXXXX (+rN) + (ba/po)
	b	_pend		
	


_p2:
	rlwinm.	r5,r3,20,31,31		#r3 = rN use bit (Z)
	beq	+12			#flag is not set(=0), address = XXXXXXXX

	lwzx	r9,r7,r9		#r9 = load register N
	add	r4,r4,r9		#flag is set (=1), address = XXXXXXXX+rN

	bge	cr4,+12
	stwx	r6,r12,r4		#[(ba/po)+XXXXXXXX] = base address
	b	_readcodes
	stwx	r16,r12,r4		#[(ba/po)+XXXXXXXX] = pointer
	b	_readcodes


_pend:
	bge	cr4,+12
	mr	r6,r4			#store result to base address
	b	_readcodes
	mr	r16,r4			#store result to pointer
	b	_readcodes	


#CT3============================================================================
#set repeat     (0): 6000ZZZZ 0000000P = set repeat
#execute repeat (1): 62000000 0000000P = execute repeat
#return		(2): 64S00000 0000000P = return (lf true/false/always)
#goto		(3): 66S0XXXX 00000000 = goto (lf true/false/always)
#gosub		(4): 68S0XXXX 0000000P = gosub (lf true/false/always)


_repeat_goto:
	rlwinm	r9,r4,3,25,28		#r9  = extract P, makes P*8
	addi	r9,r9,0x40		#offset that points to block P's
	cmpwi	r5,2			#compares sub code type with 2
	blt-	_repeat


	rlwinm.	r11,r3,10,0,1		#extract (S&3)
	beq	+20			#S=0, skip lf true, don't skip lf false
	bgt	+8
	b	_b_bl_blr_nocheck	#S=2/3, always skip (code exec status turned to true)
	beq-	cr7,_readcodes		#S=1, skip lf false, don't skip lf true
	b	_b_bl_blr_nocheck


_b_bl_blr:
	bne-	cr7,_readcodes		#lf code execution set to false skip code

_b_bl_blr_nocheck:
	cmpwi	r5,3

	bgt-	_bl			#sub code type >=4, bl
	beq+	_b			#sub code type ==3, b

_blr:
	lwzx	r15,r7,r9		#loads the next code address
	b	_readcodes

_bl:
	stwx	r15,r7,r9		#stores the next code address in block P's address
_b:
	extsh	r4,r3			#XXXX becomes signed
	rlwinm	r4,r4,3,9,28

	add	r15,r15,r4		#next code address +/-=line XXXX
	b	_readcodes



_repeat:
	bne-	cr7,_readcodes		#lf code execution set to false skip code

	add	r5,r7,r9		#r5 points to P address
	bne-	cr4,_execute_repeat	#branch lf sub code type == 1

_set_repeat:
	rlwinm	r4,r3,0,16,31		#r4  = extract NNNNN
	stw	r15,0(r5)		#store current code address to [bP's address]
	stw	r4,4(r5)		#store NNNN to [bP's address+4]

	b	_readcodes

_execute_repeat:
	lwz	r9,4(r5)		#load NNNN from [M+4]
	cmpwi	r9,0
	beq-	_readcodes
	subi	r9,r9,1
	stw	r9,4(r5)		#saves (NNNN-1) to [bP's address+4]
	lwz	r15,0(r5)		#load next code address from [bP's address]
	b	_readcodes

#CT4============================================================================
#set/add to rN(0) : 80SY000N XXXXXXXX = rN = (ba/po) + XXXXXXXX
#load rN      (1) : 82UY000N XXXXXXXX = rN = [XXXXXXXX] (offset support) (U:8/16/32)
#store rN     (2) : 84UYZZZN XXXXXXXX = store rN in [XXXXXXXX] (offset support) (8/16/32)

#operation 1  (3) : 86TY000N XXXXXXXX = operation rN?XXXXXXXX ([rN]?XXXXXXXX)
#operation 2  (4) : 88TY000N 0000000M = operation rN?rM ([rN]?rM, rN?[rM], [rN]?[rM])

#copy1        (5) : 8AYYYYNM XXXXXXXX = copy YYYY bytes from [rN] to ([rM]+)XXXXXXXX
#copy2        (6) : 8CYYYYNM XXXXXXXX = copy YYYY bytes from ([rN]+)XXXXXX to [rM]


#for copy1/copy2, lf register == 0xF, base address is used.

#of course, sub codes types 0/1, 2/3 and 4/5 can be put together lf we need more subtypes.


_operation_rN:

	bne-	cr7,_readcodes

	rlwinm	r11,r3,2,26,29		#r11  = extract N, makes N*4
	add	r26,r7,r11		#1st value address = rN's address
	lwz	r9,0(r26)		#r9 = rN

	rlwinm	r14,r3,12,30,31		#extracts S, U, T (3bits)

	beq-	cr4,_op0		#lf sub code type = 0

	cmpwi	cr4,r5,5
	bge-	cr4,_op56			#lf sub code type = 5/6

	cmpwi	cr4,r5,3
	bge-	cr4,_op34			#lf sub code type = 3/4


	cmpwi	cr4,r5,1

_op12:	#load/store
	rlwinm.	r5,r3,16,31,31		#+(ba/po) flag : Y
	beq	+8			#address = XXXXXXXX
	add	r4,r12,r4

	cmpwi	cr6,r14,1
	bne-	cr4,_store

_load:
	bgt+	cr6,+24
	beq-	cr6,+12

	lbz	r4,0(r4)		#load byte at address
	b	_store_reg

	lhz	r4,0(r4)		#load halfword at address
	b	_store_reg

	lwz	r4,0(r4)		#load word at address
	b	_store_reg

_store:

	rlwinm	r19,r3,28,20,31		#r9=r3 ror 12 (N84UYZZZ)

_storeloop:
	bgt+	cr6,+32
	beq-	cr6,+16

	stb	r9,0(r4)		#store byte at address
	addi	r4,r4,1
	b	_storeloopend

	sth	r9,0(r4)		#store byte at address
	addi	r4,r4,2
	b	_storeloopend

	stw	r9,0(r4)		#store byte at address
	icbi r0, r4 #Invalidate at offset given by storing gecko register
	sync
	isync
	addi	r4,r4,4
_storeloopend:
	subic.	r19,r19,1
	bge 	_storeloop
	b	_readcodes


_op0:	
	rlwinm.	r5,r3,16,31,31		#+(ba/po) flag : Y
	beq	+8			#value = XXXXXXXX
	add	r4,r4,r12		#value = XXXXXXXX+(ba/po)	



	andi.	r5,r14,1		#add flag : S
	beq	_store_reg		#add flag not set (=0), rN=value
	add	r4,r4,r9		#add flag set (=1), rN=rN+value
	b	_store_reg





_op34:	#operation 1 & 2


	rlwinm	r10,r3,16,30,31		#extracts Y

	rlwinm	r14,r4,2,26,29		#r14  = extract M (in r4), makes M*=4

	add	r19,r7,r14		#2nd value address = rM's address
	bne	cr4,+8
	subi	r19,r15,4		#lf CT3, 2nd value address = XXXXXXXX's address


	lwz	r4,0(r26)		#1st value = rN

	lwz	r9,0(r19)		#2nd value = rM/XXXXXXXX


	andi.	r11,r10,1		#lf [] for 1st value
	beq	+8
	mr	r26,r4			


	andi.	r11,r10,2		#lf [] for 2nd value
	beq	+16
	mr	r19,r9
	bne+	cr4,+8	
	add	r19,r12,r19		#lf CT3, 2nd value address = XXXXXXXX+(ba/op)



	rlwinm.	r5,r3,12,28,31		#operation # flag : T


	cmpwi	r5,9
	bge	_op_float


_operation_bl:
	bl	_operation_bl_return

_op450:
	add	r4,r9,r4		#N + M
	b	_store_reg

_op451:
	mullw	r4,r9,r4		#N * M
	b	_store_reg

_op452:
	or	r4,r9,r4		#N | M
	b	_store_reg

_op453:
	and	r4,r9,r4		#N & M
	b	_store_reg

_op454:
	xor	r4,r9,r4		#N ^ M
	b	_store_reg

_op455:
	slw	r4,r9,r4		#N << M
	b	_store_reg

_op456:
	srw	r4,r9,r4		#N >> M
	b	_store_reg

_op457:
	rlwnm	r4,r9,r4,0,31		#N rol M
	b	_store_reg

_op458:
	sraw	r4,r9,r4		#N asr M

_store_reg:
	stw	r4,0(r26)		#Store result in rN/[rN]
	b	_readcodes

_op_float:
	cmpwi	r5,0xA
	bgt	_readcodes

	lfs	f2,0(r26)		#f2 = load 1st value
	lfs	f3,0(r19)		#f3 = load 2nd value
	beq-	_op45A


_op459:
	fadds	f2,f3,f2		#N = N + M (float)
	b	_store_float

_op45A:
	fmuls	f2,f3,f2		#N = N * M (float)

_store_float:
	stfs	f2,0(r26)		#Store result in rN/[rN]
	b	_readcodes

_operation_bl_return:
	mflr	r10
	rlwinm	r5,r5,3,25,28		#r5 = T*8
	add	r10,r10,r5		#jumps to _op5: + r5

	lwz	r4,0(r26)		#load [rN]
	lwz	r9,0(r19)		#2nd value address = rM/XXXXXXXX

	mtlr	r10
	blr


#copy1        (5) : 8AYYYYNM XXXXXXXX = copy YYYY bytes from [rN] to ([rM]+)XXXXXXXX
#copy2        (6) : 8CYYYYNM XXXXXXXX = copy YYYY bytes from ([rN]+)XXXXXX to [rM]

_op56:				

	bne-	cr7,_readcodes		#lf code execution set to false skip code

	rlwinm	r9,r3,24,0,31		#r9=r3 ror 8 (NM8AYYYY, NM8CYYYY)
	mr	r14,r12			#r14=(ba/po)
	bl	_load_NM

	beq-	cr4,+12   
	add	r17,r17,r4		#lf sub code type==0 then source+=XXXXXXXX
	b	+8
	add	r9,r9,r4		#lf sub code type==1 then destination+=XXXXXXXX

	rlwinm.	r4,r3,24,16,31		#Extracts YYYY, compares it with 0
	li	r5,0

	_copy_loop:
	beq 	_readcodes		#Loop until all bytes have been copied.
	lbzx 	r10,r5,r17
	stbx 	r10,r5,r9
	addi	r5,r5,1
	cmpw	r5,r4
	b 	_copy_loop
	


#===============================================================================
#This is a routine called by _memory_copy and _compare_NM_16

_load_NM:				

	cmpwi	cr5,r10,4		#compare code type and 4(rn Operations) in cr5

	rlwinm 	r17,r9,6,26,29		#Extracts N*4
	cmpwi 	r17,0x3C
	lwzx	r17,r7,r17		#Loads rN value in r17
	bne 	+8
	mr	r17,r14			#lf N==0xF then source address=(ba/po)(+XXXXXXXX, CT5)

	beq	cr5,+8
	lhz	r17,0(r17)		#...and lf CT5 then N = 16 bits at [XXXXXX+base address]

	rlwinm 	r9,r9,10,26,29		#Extracts M*4
	cmpwi 	r9,0x3C
	lwzx	r9,r7,r9		#Loads rM value in r9
	bne 	+8
	mr	r9,r14			#lf M==0xF then dest address=(ba/po)(+XXXXXXXX, CT5)

	beq	cr5,+8
	lhz	r9,0(r9)		#...and lf CT5 then M = 16 bits at [XXXXXX+base address]


	blr

#CT5============================================================================
#16bits conditional (0,1,2,3): A0XXXXXX NM00YYYY (unknown values)
#16bits conditional (4,5,6,7): A8XXXXXX ZZZZYYYY (counter)

#sub codes types 0,1,2,3 compare [rN] with [rM] (both 16bits values)
#lf register == 0xF, the value at [base address+XXXXXXXX] is used.

_compare16_NM_counter:
	cmpwi 	r5,4
	bge	_compare16_counter

_compare16_NM:

	mr	r9,r4			#r9=NM00YYYY

	add	r14,r3,r12		#r14 = XXXXXXXX+(ba/po)

	rlwinm	r14,r14,0,0,30		#16bits align (base address+XXXXXXXX)

	bl	_load_NM		#r17 = N's value, r9 = M's value

	nor	r4,r4,r4		#r4=!r4
	rlwinm	r4,r4,0,16,31		#Extracts !YYYY

	and	r11,r9,r4		#r3 = (M AND !YYYY)
	and	r4,r17,r4		#r4 = (N AND !YYYY)

	b _conditional


_compare16_counter:
	rlwinm	r11,r3,28,16,31		#extract counter value from r3 in r11
	b _conditional

#===============================================================================
#execute     (0) : C0000000 NNNNNNNN = execute
#hook1       (2) : C4XXXXXX NNNNNNNN = insert instructions at XXXXXX
#hook2       (3) : C6XXXXXX YYYYYYYY = branch from XXXXXX to YYYYYY
#on/off      (6) : CC000000 00000000 = on/off switch
#range check (7) : CE000000 XXXXYYYY = is ba/po in XXXX0000-YYYY0000

_hook_execute:
	mr	r26,r4			#r18 = 0YYYYYYY
	rlwinm	r4,r4,3,0,28		#r4  = NNNNNNNN*8 = number of lines (and not number of bytes)
	bne-	cr4,_hook_addresscheck	#lf sub code type != 0
	bne-	cr7,_skip_and_align

_execute:
	mtlr	r15
	blrl

_skip_and_align:
	add	r15,r4,r15
	addi	r15,r15,7
	rlwinm	r15,r15,0,0,28		#align 64-bit
	b	_readcodes


_hook_addresscheck:
	
	cmpwi	cr4,r5,3
	bgt-	cr4,_addresscheck1	#lf sub code type ==6 or 7
	lis	r5,0x4800
	add	r12,r3,r12
	rlwinm	r12,r12,0,0,29		#align address

	bne-	cr4,_hook1		#lf sub code type ==2


_hook2:
	bne-	cr7,_readcodes

	rlwinm	r4,r26,0,0,29		#address &=0x01FFFFFC

	sub	r4,r4,r12		#r4 = to-from
	rlwimi	r5,r4,0,6,29		#r5  = (r4 AND 0x03FFFFFC) OR 0x48000000
	rlwimi	r5,r3,0,31,31		#restore lr bit
	stw	r5,0(r12)		#store opcode
	icbi r0, r12 #Invalidate at branch
	sync
	isync
	b	_readcodes

_hook1:
	bne-	cr7,_skip_and_align

	sub	r9,r15,r12		#r9 = to-from
	rlwimi	r5,r9,0,6,29		#r5  = (r9 AND 0x03FFFFFC) OR 0x48000000
	stw	r5,0(r12)		#stores b at the hook place (over original instruction)
	icbi r0, r12 #Invalidate at hook location
	sync
	isync
	addi	r12,r12,4
	add	r11,r15,r4
	subi	r11,r11,4		#r11 = address of the last word of the hook1 code
	sub	r9,r12,r11
	rlwimi	r5,r9,0,6,29		#r5  = (r9 AND 0x03FFFFFC) OR 0x48000000
	stw	r5,0(r11)		#stores b at the last word of the hook1 code
	icbi r0, r12 #Invalidate at last instruction of hook
	sync
	isync
	b	_skip_and_align


_addresscheck1:
	cmpwi	cr4,r5,6
	beq	cr4,_onoff
	b	_conditional
_addresscheck2:

	rlwinm	r12,r12,16,16,31
	rlwinm	r4,r26,16,16,31
	rlwinm	r26,r26,0,16,31
	cmpw	r12,r4
	blt	_skip
	cmpw	r12,r26
	bge	_skip
	b	_readcodes


_onoff:
	rlwinm	r5,r26,31,31,31		#extracts old exec status (x b a)
	xori	r5,r5,1
	andi.	r3,r8,1			#extracts current exec status
	cmpw	r5,r3
	beq	_onoff_end
	rlwimi	r26,r8,1,30,30
	xori	r26,r26,2



	rlwinm.	r5,r26,31,31,31		#extracts b
	beq	+8

	xori	r26,r26,1


	stw	r26,-4(r15)		#updates the code value in the code list


_onoff_end:
	rlwimi	r8,r26,0,31,31		#current execution status = a
	
	b _readcodes

#===============================================================================
#Full terminator  (0) = E0000000 XXXXXXXX = full terminator
#Endlfs/Else      (1) = E2T000VV XXXXXXXX = endlfs (+else)
#End code handler     = F0000000 00000000

_terminator_onoff_:
	cmpwi	r11,0			#lf code type = 0xF
    beq _notTerminator
    cmpwi r5,1
    beq _asmTypeba
    cmpwi r5,2
    beq _asmTypepo
    cmpwi r5,3
    beq _patchType
    b _exitcodehandler
    _asmTypeba:
    rlwinm r12,r6,0,0,6 # use base address
    _asmTypepo:
    rlwinm r23,r4,8,24,31 # extract number of half words to XOR
    rlwinm r24,r4,24,16,31 # extract XOR checksum
    rlwinm r4,r4,0,24,31 # set code value to number of ASM lines only
    bne cr7,_goBackToHandler #skip code if code execution is set to false
    rlwinm. r25,r23,0,24,24 # check for negative number of half words
    mr r26,r12 # copy ba/po address
    add r26,r3,r26 # add code offset to ba/po code address
    rlwinm r26,r26,0,0,29 # clear last two bits to align address to 32-bit
    beq _positiveOffset # if number of half words is negative, extra setup needs to be done
    extsb r23,r23
    neg r23,r23
    mulli r25,r23,2
    addi r25,r25,4
    subf r26,r25,r26
    _positiveOffset:
    cmpwi r23,0
    beq _endXORLoop
    li r25,0
    mtctr r23
    _XORLoop:
        lhz r27,4(r26)
        xor r25,r27,r25
        addi r26,r26,2
        bdnz _XORLoop
    _endXORLoop:
    cmpw r24,r25
    bne _goBackToHandler
    b _hook_execute
    _patchType:
    rlwimi	r8,r8,1,0,30		#r8<<1 and current execution status = old execution status
    bne	cr7,_exitpatch		#lf code execution is set to false -> exit
    rlwinm. r23,r3,22,0,1
    bgt _patchfail
    blt _copytopo
    _runpatch:
    rlwinm r30,r3,0,24,31
    mulli r30,r30,2
    rlwinm r23,r4,0,0,15
    xoris r24,r23,0x8000
    cmpwi r24,0
    bne- _notincodehandler
    ori r23,r23,0x3000
    _notincodehandler:
    rlwinm r24,r4,16,0,15
    mulli r25,r30,4
    subf r24,r25,r24
    _patchloop:
    li r25,0
    _patchloopnext:
    mulli r26,r25,4
    lwzx r27,r15,r26
    lwzx r26,r23,r26
    addi r25,r25,1
    cmplw r23,r24
    bgt _failpatchloop
    cmpw r25,r30
    bgt _foundaddress
    cmpw r26,r27
    beq _patchloopnext
    addi r23,r23,4
    b _patchloop
    _foundaddress:
    lwz r3,-8(r15)
    ori r3,r3,0x300
    stw r3,-8(r15)
    stw r23,-4(r15)
    mr r16,r23
    b _exitpatch
    _failpatchloop:
    lwz r3,-8(r15)
    ori r3,r3,0x100
    stw r3,-8(r15)
    _patchfail:
    ori	r8,r8,1			#r8|=1 (execution status set to false)
    b _exitpatch
    _copytopo:
    mr r16,r4
    _exitpatch:
    rlwinm r4,r3,0,24,31 # set code to number of lines only
    _goBackToHandler:
    mulli r4,r4,8
    add r15,r4,r15 # skip the lines of the code
    b _readcodes
    _notTerminator:

_terminator:
	bne	cr4,+12			#check lf sub code type == 0
	li	r8,0			#clear whole code execution status lf T=0
	b	+20

	rlwinm.	r9,r3,0,27,31		#extract VV
#	bne 	+8			#lf VV!=0
#	bne-	cr7,+16

	rlwinm	r5,r3,12,31,31		#extract "else" bit

	srw	r8,r8,r9		#r8>>VV, meaning endlf VV lfs

    rlwinm. r23,r8,31,31,31
    bne +8 # execution is false if code execution >>, so don't invert code status
	xor	r8,r8,r5		#lf 'else' is set then invert current code status

_load_baseaddress:
	rlwinm.	r5,r4,0,0,15
	beq	+8
	mr	r6,r5			#base address = r4
	rlwinm.	r5,r4,16,0,15
	beq	+8
	mr	r16,r5			#pointer = r4
	b	_readcodes

#===============================================================================
       
frozenvalue:	#frozen value, then LR
.long        0,0
dwordbuffer:
.long        0,0
rem:
.long        0
bpbuffer:
.long 0		#int address to bp on
.long 0		#data address to bp on
.long 0		#alignement check
.long 0		#counter for alignement

regbuffer:
.space 72*4

.align 3

codelist:
.space 2*4
.end


