
#include "Defines.h"
#include "Instructions.h"

enum Modify
{
	MOD_CR0 = (1 << 0),
	MOD_CR1 = (1 << 1),
	MOD_SR = (1 << 2),
	MOD_XER = (1 << 3),
	MOD_FPSCR = (1 << 4)
};
enum IOput
{
	IO_ARG1 = (1 << 0),
	IO_ARG2 = (1 << 1),
	IO_ARG3 = (1 << 2),
};
enum inst_type
{
	TYPE_INTEGER,
	TYPE_FLOAT
};

struct inst
{
	char name[16];
	u32 Modifies; // Flag modification
	u8 numInput;
	u32 Input;
	u32 Ouput;
	inst_type type;
	void (*Call)(u32*, u32*, u32*, u32*);
	void (*CallFP)(float*, float*, float*, float*);

};

static inst instructions[] = {
	{ "add", NULL, 3, IO_ARG1 | IO_ARG2 | IO_ARG3, IO_ARG1,TYPE_INTEGER, add},
	{ "add.", MOD_CR0, 3, IO_ARG1 | IO_ARG2 | IO_ARG3, IO_ARG1,TYPE_INTEGER, add},
	{ "subfc", NULL, 3, IO_ARG1 | IO_ARG2 | IO_ARG3, IO_ARG1,TYPE_INTEGER, subfc},
	{ "subfc.", MOD_CR0, 3, IO_ARG1 | IO_ARG2 | IO_ARG3, IO_ARG1,TYPE_INTEGER, subfcRC},
	{ "divw", NULL, 3, IO_ARG1 | IO_ARG2 | IO_ARG3, IO_ARG1, TYPE_INTEGER, divw},
	{ "divw.", MOD_CR0, 3, IO_ARG1 | IO_ARG2 | IO_ARG3, IO_ARG1,TYPE_INTEGER, divwRC},
	{ "divwo", MOD_XER, 3, IO_ARG1 | IO_ARG2 | IO_ARG3, IO_ARG1,TYPE_INTEGER, divwo},
	{ "divwo.", MOD_CR0 | MOD_XER, 3, IO_ARG1 | IO_ARG2 | IO_ARG3, IO_ARG1, TYPE_INTEGER, divwoRC},
	//{ "fsqrt", MOD_FPSCR, 2, IO_ARG1 | IO_ARG2, IO_ARG1,TYPE_FLOAT, NULL, fsqrt},
	//{ "fsqrt.", MOD_CR1 | MOD_FPSCR, 2, IO_ARG1 | IO_ARG2, IO_ARG1,TYPE_FLOAT, NULL, fsqrtRC}
};


void RunInstruction(u32 inst);
