#include "asm_tables.h"
#include "Init.h"
#include "Helpers.h"

// Limited Test, Later have a test that will pass all possible variables
u32 inval_table[][2] = {
	{0,0},
	{1,1},
	{300,0},
	{1,0},
	{0xffff,0},
	{0xffffffff,0},
	{0x80000000,0},
	{0x80000000,1},
	{0x80000000,2},
	{0x7fffffff,0},
	{0x7fffffff,1},
	{0x7fffffff,2},
	{0,1},
	{0,0xffff},
	{0,0xffffffff},
	{0,0x80000000},
	{1,0x80000000},
	{2,0x80000000},
	{0,0x7fffffff},
	{1,0x7fffffff},
	{2,0x7fffffff},
	{654321,0},
	{4653321,0},
	{0,300},
	{1024,8},
	{8,1024},
	{0xffff,0xffff},
	{0xffffffff,0xffffffff}
};
void Print(const char* text)
{
	printf(text);
	if(f)
		fprintf(f, text);
}
void ShowModifies(u32 inst)
{
	u32 mod = instructions[inst].Modifies;
	if(mod == 0)
	{
		Print("NONE");
		return;
	}
	if(mod & MOD_CR0)
		Print("CR0 ");
	if(mod & MOD_CR1)
		Print("CR1 ");
	if(mod & MOD_SR)
		Print("SR ");
	if(mod & MOD_XER)
		Print("XER ");
}
void RunInstruction(u32 inst)
{
	u32 inval1, inval2, inval3;

	u32 outval = 0;

	// CR0
	u32 cr1 = 0, cr2 = 0;

	//CR1
	u32 cr11 = 0, cr12 = 0;

	//XER
	u32 xer1 = 0, xer2 = 0;

	bool modCR0 = instructions[inst].Modifies & MOD_CR0;
	bool modCR1 = instructions[inst].Modifies & MOD_CR1;
	bool modXER = instructions[inst].Modifies & MOD_XER;

	if(instructions[inst].numInput != 3)
	{
		Print("Don't support Input not 3 yet~!\n");
		fclose(f);
		return;
	}
	if(instructions[inst].type != TYPE_INTEGER)
	{
		Print("Types other than TYPE_INTEGER not supported yet!\n");
		fclose(f);
		return;
	}
	char temp[32];
	sprintf(temp, "logs/%s.dolphin.jit.log", instructions[inst].name);
	f = fopen(temp, "wb");
	if (!f)
		printf("unable to open output file\n");

	printf("%s: InputNum: %d Modifies(flags): ", instructions[inst].name, instructions[inst].numInput);
	if(f)
		fprintf(f, "%s: InputNum: %d Modifies(flags): ", instructions[inst].name, instructions[inst].numInput);
	ShowModifies(inst);
	Print("\n");

	for (unsigned int i = 0; i < sizeof(inval_table)/(sizeof(int)*2); i++)
	{
		inval1 = inval_table[i][0];
		inval2 = inval_table[i][1];
		outval = 0;

		// Show our input values and where we are at on the array
		printf("\x1b[%i;0H", i);
		printf("%07i: %08x,%08x",i, inval1,inval2);

		// Get flags before
		if(modCR0)
			cr1 = GetCR0();
		if(modCR1)
			cr11 = GetCR(1);
		if(modXER)
			xer1 = GetXER();

		//Actually call instruction
		instructions[inst].Call(&outval, &inval1, &inval2, 0);

		// Get flags after
		if(modCR0)
			cr2 = GetCR0();
		if(modCR1)
			cr12 = GetCR(1);
		if(modXER)
			xer2 = GetXER();

		// Print out value
		printf(":o=%08x\n\t", outval);

		// show off flag changes
		if(modCR0)
			printf("CR0:(%08x ~ %08x)", cr1,cr2);
		if(modCR1)
			printf("CR1:(%08x ~ %08x)", cr11,cr12);
		if(modXER)
			printf("XER:(%08x ~ %08x)", xer1, xer2);


		// same in the file
		if(f)
		{
			fprintf(f, ":i=%08x, %08x:o=%08x\n\t", inval1,inval2, outval);
			if(modCR0)
				fprintf(f, "CR0:(%08x ~ %08x)", cr1,cr2);
			if(modCR1)
				fprintf(f, "CR1:(%08x ~ %08x)", cr11,cr12);
			if(modXER)
				fprintf(f, "XER:(%08x ~ %08x)", xer1, xer2);
		}

		// see the difference in flags if any
		if(modCR0)
		{
			u32 cr_diff = cr2&~cr1;
			if (cr_diff) {
				printf(" CR0D:%08x",cr_diff);
				if(f)
					fprintf(f, " CR0D:%08x",cr_diff);
			}
		}
		if(modCR1)
		{
			u32 cr1_diff = cr12&~cr11;
			if (cr1_diff) {
				printf(" CR1D:%08x",cr1_diff);
				if(f)
					fprintf(f, " CR1D:%08x",cr1_diff);
			}
		}
		if(modXER)
		{
			u32 xer_diff = xer2&~xer1;
			if (xer_diff) {
				printf(" XERD:%08x",xer_diff);
				if(f)
					fprintf(f, " XERD:%08x",xer_diff);
			}
		}
		if(f)
			fprintf(f,"\n");
	}
	if(f)
		fclose(f);
}
