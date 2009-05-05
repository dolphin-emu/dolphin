#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

typedef unsigned short u16;

u16 reg_in[32], reg_out[1000][32];

void printRegs(u16 *lastRegs, u16 *regs) {
	for (int i = 0; i < 32; i++) {
		if (! lastRegs || lastRegs[i] != regs[i]) {
			printf("%02x %04x  ", i, htons(regs[i]));
		} else {
			printf("         ");
		}
		
		if ((i+1) % 8 == 0)
			printf("\n");
	}
}

int main(int argc, char **argv) {

	if (argc != 2) {
		fprintf(stderr, "Usage %s: <filename>\n", argv[0]);
		exit(1);
	}

	FILE *f = fopen(argv[1], "rb");
	int steps;
	if (f) {
		// read initial regs 
		fread(reg_in, 1, 32 * 2, f);
		
		// read initial regs (guess number of steps)
		steps = fread(reg_out, 32 * 2, 1000, f);
		fclose(f);
	} else {
		fprintf(stderr, "Error opening file %s\n", argv[1]);
		exit(1);
	}

	printf("Start with:\n");
	printRegs(NULL, reg_in);
	
	printf("\nStep 0:\n");
	printRegs(reg_in, reg_out[0]);

	for (int i=1;i < steps;i++) {
		printf("\nStep %d:\n", i);
		printRegs(reg_out[i-1], reg_out[i]);
	}

	exit(0);
}
