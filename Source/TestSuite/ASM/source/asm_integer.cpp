#include "asm_tables.h"
#include "Defines.h"

void add(u32 *a, u32 *b, u32 *c, u32 *d)
{
	asm(
		"add %0,%1,%2"
		: "=r"(*a)
		: "r"(*b), "r"(*c)
		);
}
void addRC(u32 *a, u32 *b, u32 *c, u32 *d)
{
	asm(
		"add. %0,%1,%2"
		: "=r"(*a)
		: "r"(*b), "r"(*c)
		: "cc"
		);
}

void subfc(u32 *a, u32 *b, u32 *c, u32 *d)
{
	asm(
		"subfc %0,%1,%2"
		: "=r"(*a)
		: "r"(*b), "r"(*c)
		);
}
void subfcRC(u32 *a, u32 *b, u32 *c, u32 *d)
{
	asm(
		"subfc. %0,%1,%2"
		: "=r"(*a)
		: "r"(*b), "r"(*c)
		: "cc"
		);
}

void divw(u32 *a, u32 *b, u32 *c, u32 *d)
{
	asm(
		"divw %0,%1,%2"
		: "=r"(*a)
		: "r"(*b), "r"(*c)
		);
}
void divwRC(u32 *a, u32 *b, u32 *c, u32 *d)
{
	asm(
		"divw. %0,%1,%2"
		: "=r"(*a)
		: "r"(*b), "r"(*c)
		: "cc"
		);
}

void divwo(u32 *a, u32 *b, u32 *c, u32 *d)
{
	asm(
		"divwo %0,%1,%2"
		: "=r"(*a)
		: "r"(*b), "r"(*c)
		);
}
void divwoRC(u32 *a, u32 *b, u32 *c, u32 *d)
{
	asm(
		"divwo. %0,%1,%2"
		: "=r"(*a)
		: "r"(*b), "r"(*c)
		: "cc"
		);
}