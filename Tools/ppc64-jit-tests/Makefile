# Builds the guest CPU test program, which runs on Dolphin to compare its CPU cores.
# Needs a 32-bit big-endian PowerPC cross compiler, e.g. Fedora's gcc-powerpc64-linux-gnu.
#
#   make
#   ./run.sh path/to/dolphin-emu-nogui cputest.elf 0 > interpreter.txt
#   ./run.sh path/to/dolphin-emu-nogui cputest.elf 6 > jit.txt
#   diff interpreter.txt jit.txt

CC = powerpc64-linux-gnu-gcc
CFLAGS = -m32 -mcpu=750 -mbig-endian -O2 -ffreestanding -fno-builtin -nostdlib -fno-pic \
	-fno-stack-protector -fno-asynchronous-unwind-tables -Wa,-mgekko
LDFLAGS = -static -Wl,-T,link.ld -Wl,--build-id=none -Wl,-z,noexecstack

all: cputest.elf cputest-jit.elf cputest-jit-fprf.elf cputest-mmu.elf cputest-timing.elf fuzz.elf

cputest.elf: crt0.S cputest.c link.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.S cputest.c

# For comparing the JITs, which don't update the FPSCR's status bits, with the interpreter. Run
# the FPRF one with -C Dolphin.Core.FPRF=True.
cputest-jit.elf: crt0.S cputest.c link.ld
	$(CC) $(CFLAGS) -DFPSCR_MASK=0xFFu $(LDFLAGS) -o $@ crt0.S cputest.c

cputest-jit-fprf.elf: crt0.S cputest.c link.ld
	$(CC) $(CFLAGS) -DFPSCR_MASK=0x1F0FFu $(LDFLAGS) -o $@ crt0.S cputest.c

# Also tests data storage exceptions, which need MMU mode: run it with -C Dolphin.Core.MMU=True.
# The JIT keeps page table mappings until tlbie with page table fastmem, so its report of the
# TLB's replacement, "page-table tlb set", only matches with -C Dolphin.Core.PageTableFastmem=False.
cputest-mmu.elf: crt0.S cputest.c link.ld
	$(CC) $(CFLAGS) -DFPSCR_MASK=0xFFu -DTEST_DSI $(LDFLAGS) -o $@ crt0.S cputest.c

# Only the timing test, whose report depends on how a core forms blocks: compare a JIT's with its
# report from a build known to be good. Run it with -C Dolphin.Core.CPUThread=False.
cputest-timing.elf: crt0.S cputest.c link.ld
	$(CC) $(CFLAGS) -DFPSCR_MASK=0xFFu -DTEST_TIMING $(LDFLAGS) -o $@ crt0.S cputest.c

# The fuzzer, which fuzz.py runs.
fuzz.elf: crt0.S fuzz_run.S fuzz.c link.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.S fuzz_run.S fuzz.c

clean:
	rm -f cputest.elf cputest-jit.elf cputest-jit-fprf.elf cputest-mmu.elf cputest-timing.elf fuzz.elf
