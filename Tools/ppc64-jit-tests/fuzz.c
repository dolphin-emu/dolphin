// A differential fuzzer for Dolphin's CPU cores: it generates programs of random instructions,
// runs each from a random state, and reports a hash of the registers and memory it leaves, so that
// running it on two cores and comparing the reports finds programs that they run differently.
// fuzz.py runs it and reduces the programs that differ.
//
// The programs are integer, CR, XER/CTR, floating-point and paired-single instructions, loads and
// stores of every form to a buffer, string and cache instructions, and forward branches, many of
// them right after the compare they test. They don't write r1 or r31, which points at the state,
// nor LR, MSR or the GQRs, and they leave out what the JITs don't emulate exactly, FPSCR's status
// bits: mffs and mcrfs, and floating-point record forms but after clearing them. Instructions that
// set up an address, or clear the status bits, are glued to the one they're for: they stay when
// it does, whichever fuzz.py replaces with nop.
//
// Traps and system calls raise exceptions, and in MMU mode accesses to pages of a segment that the
// page table maps or not raise DSI exceptions; their handlers count them and return after the
// instruction, as do the alignment exception's, which nothing should raise.

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef int s32;

// Reports go through OSReport, which Dolphin replaces with a printf to its log in debug mode.
// noipa: Dolphin reads the arguments, so the compiler must not assume that this function doesn't.
void OSReport(const char* format, ...) __attribute__((noipa, used));
void OSReport(const char* format, ...)
{
  asm volatile("" : : : "memory");
}

#define MAX_LENGTH 128

// Set by fuzz.py in the ELF file: the programs to run, whether to report their code and state in
// full, and for reducing one, which, and its instructions to replace with nop.
struct Config
{
  u32 magic[2];
  u32 start;
  u32 count;
  u32 dump;
  u32 reduced;
  u32 flags;
  u32 nop[MAX_LENGTH / 32];
};
volatile struct Config config __attribute__((used)) = {
    {0x46555A5A, 0x434F4E46}, 0, 2000, 0, 0xFFFFFFFF, 0, {0}};
// Dolphin runs in MMU mode, and with FPRF.
#define FLAG_MMU 1
#define FLAG_FPRF 2

// See fuzz_run.S.
struct State
{
  u32 gpr[32];
  u64 ps0[32];
  u32 ps1[32];
  u32 cr, xer, ctr, pad0;
  u64 fpscr, pad1;
  u32 out_gpr[32];
  u64 out_ps0[32];
  u64 out_ps1[32];
  u32 out_cr, out_xer, out_ctr, pad2;
  u64 out_fpscr;
  u64 zero;
  // The exception counts, and the DAR and DSISR of the last DSI exception.
  u32 out_exceptions, out_dar, out_dsisr, pad3;
  u32 pad4[4];
  u8 buffer[1024];
} __attribute__((aligned(32)));
_Static_assert(__builtin_offsetof(struct State, cr) == 512, "layout");
_Static_assert(__builtin_offsetof(struct State, fpscr) == 528, "layout");
_Static_assert(__builtin_offsetof(struct State, out_gpr) == 544, "layout");
_Static_assert(__builtin_offsetof(struct State, out_cr) == 1184, "layout");
_Static_assert(__builtin_offsetof(struct State, out_fpscr) == 1200, "layout");
_Static_assert(__builtin_offsetof(struct State, zero) == 1208, "layout");
_Static_assert(__builtin_offsetof(struct State, out_exceptions) == 1216, "layout");
// Aligned for dcbz.
#define BUFFER 1248
_Static_assert(__builtin_offsetof(struct State, buffer) == BUFFER, "layout");

void FuzzRun(struct State* state, const u32* code);

// Exception handlers, copied to the vectors: they count the exceptions in SPRG1, a byte for each
// kind, and but for system calls return after the instruction.
extern const u32 dsi_handler[], dsi_handler_end[], alignment_handler[], alignment_handler_end[];
extern const u32 program_handler[], program_handler_end[], syscall_handler[], syscall_handler_end[];
asm(".section .text\n"
    "dsi_handler:\n"
    "  mtsprg0 3\n  mfsprg1 3\n  addi 3,3,1\n  mtsprg1 3\n"
    "  mfdar 3\n  mtsprg2 3\n  mfdsisr 3\n  mtsprg3 3\n"
    "  mfsrr0 3\n  addi 3,3,4\n  mtsrr0 3\n  mfsprg0 3\n  rfi\n"
    "dsi_handler_end:\n"
    "alignment_handler:\n"
    "  mtsprg0 3\n  mfsprg1 3\n  addis 3,3,0x100\n  mtsprg1 3\n  mfdar 3\n  mtsprg2 3\n"
    "  mfsrr0 3\n  addi 3,3,4\n  mtsrr0 3\n  mfsprg0 3\n  rfi\n"
    "alignment_handler_end:\n"
    "program_handler:\n"
    "  mtsprg0 3\n  mfsprg1 3\n  addi 3,3,0x100\n  mtsprg1 3\n  mfsrr1 3\n  mtsprg3 3\n"
    "  mfsrr0 3\n  addi 3,3,4\n  mtsrr0 3\n  mfsprg0 3\n  rfi\n"
    "program_handler_end:\n"
    "syscall_handler:\n"
    "  mtsprg0 3\n  mfsprg1 3\n  addis 3,3,1\n  mtsprg1 3\n  mfsprg0 3\n  rfi\n"
    "syscall_handler_end:\n");

// In MMU mode, the pages of segment 6 that programs access, which the page table maps to pages
// of these, in their first 512 bytes and their last 64.
#define SEGMENT 0x60000000
#define SEGMENT_VSID 0x123456
#define SEGMENT_PAGES 8
static u8 pages[SEGMENT_PAGES][4096] __attribute__((aligned(4096)));
// The page table: 64 KiB at physical 16 MiB.
#define PAGE_TABLE ((u32*)0x81000000)
static u32* PTE(u32 page)
{
  const u32 hash = SEGMENT_VSID ^ page;
  return PAGE_TABLE + ((hash << 6) & 0xFFC0) / 4;
}

static struct State state;
static u32 code[MAX_LENGTH + 1] __attribute__((aligned(32)));
static u32 generated[MAX_LENGTH];
static u8 glue[MAX_LENGTH];

static u32 rng;
static u32 R(void)
{
  u32 x = rng;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return rng = x;
}

static u32 Value(void)
{
  static const u32 edge[] = {0,      1,      2,          0x7FFFFFFF, 0x80000000, 0xFFFFFFFF,
                             0xFFFFFFFE, 0x8000, 0x7FFF, 0xFFFF,     0x10000,    31,
                             32,     0x80000001, 0x55555555, 0xFFFF8000};
  return (R() & 1) ? edge[R() % 16] : R();
}

static u64 DoubleValue(void)
{
  static const u64 edge[] = {
      0,                     0x8000000000000000ull, 0x3FF0000000000000ull, 0xBFF0000000000000ull,
      0x7FF0000000000000ull, 0xFFF0000000000000ull, 0x7FF8000000000000ull, 0x7FF0000000000001ull,
      0x0000000000000001ull, 0x000FFFFFFFFFFFFFull, 0x3810000000000000ull, 0x380FFFFFFFFFFFFFull,
      0x47EFFFFFE0000000ull, 0x47EFFFFFF0000000ull, 0x41E0000000000000ull, 0xC1E0000000200000ull,
      0x3FF0000010000000ull, 0x3FF0000008000000ull, 0x7FF8000010000000ull, 0x36A0000000000000ull};
  switch (R() % 4)
  {
  case 0:
    return edge[R() % 20];
  case 1:
    return ((u64)R() << 32) | R();
  default:
  {
    // A single, as a double.
    const u64 exponent = 1023 - 30 + R() % 61;
    return ((u64)(R() & 1) << 63) | (exponent << 52) | ((u64)(R() & 0x7FFFFF) << 29);
  }
  }
}

static u32 SingleValue(void)
{
  static const u32 edge[] = {0,          0x80000000, 0x3F800000, 0xBF800000, 0x7F800000,
                             0xFF800000, 0x7FC00000, 0x7F800001, 0x00000001, 0x007FFFFF,
                             0x00800000, 0x7F7FFFFF, 0x4F000000, 0xCF000000, 0x3F800001};
  switch (R() % 3)
  {
  case 0:
    return edge[R() % 15];
  case 1:
    return R();
  default:
    return ((R() & 1) << 31) | ((127 - 20 + R() % 41) << 23) | (R() & 0x7FFFFF);
  }
}

// A register that a program may write: not r1 or r31.
static u32 Dest(void)
{
  const u32 r = R() % 30;
  return r == 0 ? 0 : r + 1;
}
static u32 Reg(void)
{
  return R() % 32;
}
static u32 Imm16(void)
{
  static const u32 edge[] = {0, 1, 0xFFFF, 0x8000, 0x7FFF, 0x7FFE, 0x8001, 0xFFFE};
  return (R() & 1) ? edge[R() % 8] : R() & 0xFFFF;
}

#define D(op, t, a, imm) (((op) << 26) | ((t) << 21) | ((a) << 16) | ((imm) & 0xFFFF))
#define X(op, t, a, b, xo, rc)                                                                     \
  (((op) << 26) | ((t) << 21) | ((a) << 16) | ((b) << 11) | ((xo) << 1) | (rc))
#define A(op, t, a, b, c, xo, rc)                                                                  \
  (((op) << 26) | ((t) << 21) | ((a) << 16) | ((b) << 11) | ((c) << 6) | ((xo) << 1) | (rc))
#define SPR(n) ((((n) & 0x1F) << 5) | ((n) >> 5))

// A conditional branch on CR field crf, forward from index i, to at most the end.
static u32 BranchOn(u32 crf, u32 i, u32 length)
{
  static const u32 bo[] = {12, 4, 12, 4, 8, 0};
  u32 target = i + 1 + R() % 6;
  if (target > length)
    target = length;
  const u32 b = crf * 4 + R() % 4;
  if (R() % 4 == 0)
  {
    // A conditional return, which ends the program.
    return X(19, bo[R() % 2], b, 0, 16, 0);
  }
  return (16u << 26) | (bo[R() % 6] << 21) | (b << 16) | (((target - i) * 4) & 0xFFFC);
}

// Generates instruction i, and maybe i+1, of a program; returns how many.
static u32 GenerateBasic(u32* out, u32 i, u32 length)
{
  const u32 rc = R() & 1;
  switch (R() % 16)
  {
  case 0:
  {
    static const u32 xo[] = {266, 10, 138, 40, 8, 136, 235, 75, 11, 491, 459};
    const u32 op = xo[R() % 11];
    const u32 oe = (op == 75 || op == 11) ? 0 : (R() & 1);
    out[0] = X(31, Dest(), Reg(), Reg(), op | (oe << 9), rc);
    return 1;
  }
  case 1:
  {
    static const u32 xo[] = {234, 202, 232, 200, 104};
    out[0] = X(31, Dest(), Reg(), 0, xo[R() % 5] | ((R() & 1) << 9), rc);
    return 1;
  }
  case 2:
  {
    static const u32 op[] = {14, 15, 12, 13, 8, 7};
    out[0] = D(op[R() % 6], Dest(), Reg(), Imm16());
    return 1;
  }
  case 3:
  {
    static const u32 op[] = {24, 25, 26, 27, 28, 29};
    out[0] = D(op[R() % 6], Reg(), Dest(), Imm16());
    return 1;
  }
  case 4:
  {
    static const u32 xo[] = {28, 60, 444, 412, 316, 476, 124, 284, 24, 536, 792};
    switch (R() % 3)
    {
    case 0:
      out[0] = X(31, Reg(), Dest(), Reg(), xo[R() % 11], rc);
      break;
    case 1:
      out[0] = X(31, Reg(), Dest(), R() % 32, 824, rc);  // srawi
      break;
    default:
    {
      static const u32 unary[] = {954, 922, 26};
      out[0] = X(31, Reg(), Dest(), 0, unary[R() % 3], rc);
      break;
    }
    }
    return 1;
  }
  case 5:
  {
    static const u32 op[] = {21, 20, 23};
    const u32 o = op[R() % 3];
    const u32 b = o == 23 ? Reg() : R() % 32;
    out[0] = (o << 26) | (Reg() << 21) | (Dest() << 16) | (b << 11) | ((R() % 32) << 6) |
             ((R() % 32) << 1) | rc;
    return 1;
  }
  case 6:
  case 7:
  {
    // A compare, often with a branch on it right after.
    const u32 crf = R() % 8;
    switch (R() % 4)
    {
    case 0:
      out[0] = X(31, crf << 2, Reg(), Reg(), 0, 0);
      break;
    case 1:
      out[0] = X(31, crf << 2, Reg(), Reg(), 32, 0);
      break;
    case 2:
      out[0] = D(11, crf << 2, Reg(), Imm16());
      break;
    default:
      out[0] = D(10, crf << 2, Reg(), Imm16());
      break;
    }
    if (i + 1 < length && (R() & 1))
    {
      out[1] = BranchOn(crf, i + 1, length);
      return 2;
    }
    return 1;
  }
  case 8:
  {
    // CR and SPR moves.
    static const u32 cr_xo[] = {257, 129, 289, 225, 33, 449, 417, 193};
    switch (R() % 9)
    {
    case 0:
    case 1:
      out[0] = X(19, R() % 32, R() % 32, R() % 32, cr_xo[R() % 8], 0);
      break;
    case 2:
      out[0] = X(19, (R() % 8) << 2, (R() % 8) << 2, 0, 0, 0);  // mcrf
      break;
    case 3:
      out[0] = X(31, (R() % 8) << 2, 0, 0, 512, 0);  // mcrxr
      break;
    case 4:
      out[0] = X(31, Dest(), 0, 0, 19, 0);  // mfcr
      break;
    case 5:
      out[0] = (31u << 26) | (Reg() << 21) | ((R() & 0xFF) << 12) | (144 << 1);  // mtcrf
      break;
    case 6:
      out[0] = (31u << 26) | (Reg() << 21) | (SPR(R() & 1 ? 1 : 9) << 11) | (467 << 1);
      break;
    default:
      out[0] = (31u << 26) | (Dest() << 21) | (SPR(R() & 1 ? 1 : 9) << 11) | (339 << 1);
      break;
    }
    return 1;
  }
  case 9:
  {
    // Integer loads and stores to the buffer.
    static const u32 op[] = {32, 34, 40, 42, 36, 38, 44};
    const u32 o = op[R() % 7];
    const u32 t = o >= 36 && o != 40 && o != 42 ? Reg() : Dest();
    out[0] = D(o, t, 31, BUFFER + R() % (1024 - 8));
    return 1;
  }
  case 10:
  {
    // Floating-point and quantized loads and stores.
    if (R() & 1)
    {
      static const u32 op[] = {48, 50, 52, 54};
      out[0] = D(op[R() % 4], Reg(), 31, BUFFER + (R() % 120) * 8);
    }
    else
    {
      const u32 op = (R() & 1) ? 56 : 60;
      const u32 offset = BUFFER + (R() % 100) * 8;
      out[0] = (op << 26) | (Reg() << 21) | (31 << 16) | ((R() & 1) << 15) | ((R() % 8) << 12) |
               (offset & 0xFFF);
    }
    return 1;
  }
  case 11:
  {
    // Double-precision arithmetic.
    static const u32 xo[] = {21, 20, 25, 18, 29, 28, 31, 30, 23, 26};
    const u32 x = xo[R() % 10];
    const u32 a = x == 26 ? 0 : Reg();
    const u32 b = x == 25 ? 0 : Reg();
    const u32 c = x == 25 || x >= 28 || x == 23 ? Reg() : 0;
    out[0] = A(63, Reg(), a, b, c, x, 0);
    return 1;
  }
  case 12:
  {
    // Single-precision arithmetic.
    static const u32 xo[] = {21, 20, 25, 18, 29, 28, 31, 30, 24};
    const u32 x = xo[R() % 9];
    const u32 a = x == 24 ? 0 : Reg();
    const u32 b = x == 25 ? 0 : Reg();
    const u32 c = x == 25 || x >= 28 ? Reg() : 0;
    out[0] = A(59, Reg(), a, b, c, x, 0);
    return 1;
  }
  case 13:
  {
    // Moves, conversions, compares, and FPSCR's rounding mode and NI.
    switch (R() % 6)
    {
    case 0:
    case 1:
    {
      static const u32 xo[] = {40, 72, 136, 264, 12, 14, 15};
      out[0] = X(63, Reg(), 0, Reg(), xo[R() % 7], 0);
      return 1;
    }
    case 2:
    case 3:
    {
      const u32 crf = R() % 8;
      out[0] = X(63, crf << 2, Reg(), Reg(), (R() & 1) ? 32 : 0, 0);
      if (i + 1 < length && (R() & 1))
      {
        out[1] = BranchOn(crf, i + 1, length);
        return 2;
      }
      return 1;
    }
    case 4:
      out[0] = (63u << 26) | (7 << 23) | ((R() % 8) << 12) | (134 << 1);  // mtfsfi 7
      return 1;
    default:
      out[0] = X(63, 29 + R() % 3, 0, 0, (R() & 1) ? 38 : 70, 0);  // mtfsb1, mtfsb0
      return 1;
    }
  }
  case 14:
  {
    // Paired-single arithmetic.
    static const u32 xo[] = {21, 20, 25, 18, 29, 28, 31, 30, 10, 11, 12, 13, 14, 15, 23, 24, 26};
    const u32 x = xo[R() % 17];
    const u32 a = x == 24 || x == 26 ? 0 : Reg();
    const u32 b = x == 25 || x == 12 || x == 13 ? 0 : Reg();
    const u32 c = x == 25 || x == 12 || x == 13 || (x >= 28 && x <= 31) || x == 10 || x == 11 ||
                          x == 14 || x == 15 || x == 23 ?
                      Reg() :
                      0;
    out[0] = A(4, Reg(), a, b, c, x, 0);
    return 1;
  }
  default:
  {
    // Paired-single moves, merges and compares, and branches.
    switch (R() % 5)
    {
    case 0:
    {
      static const u32 xo[] = {40, 72, 136, 264, 528, 560, 592, 624};
      const u32 x = xo[R() % 8];
      out[0] = X(4, Reg(), x >= 528 ? Reg() : 0, Reg(), x, 0);
      return 1;
    }
    case 1:
    {
      const u32 crf = R() % 8;
      out[0] = X(4, crf << 2, Reg(), Reg(), (R() % 4) * 32, 0);
      if (i + 1 < length && (R() & 1))
      {
        out[1] = BranchOn(crf, i + 1, length);
        return 2;
      }
      return 1;
    }
    case 2:
    {
      // Decrementing CTR.
      static const u32 bo[] = {16, 18, 8, 0, 10, 2};
      u32 target = i + 1 + R() % 6;
      if (target > length)
        target = length;
      out[0] = (16u << 26) | (bo[R() % 6] << 21) | ((R() % 32) << 16) | (((target - i) * 4) & 0xFFFC);
      return 1;
    }
    case 3:
    {
      u32 target = i + 1 + R() % 6;
      if (target > length)
        target = length;
      out[0] = (18u << 26) | (((target - i) * 4) & 0x3FFFFFC);
      return 1;
    }
    default:
      out[0] = BranchOn(R() % 8, i, length);
      return 1;
    }
  }
  }
}

// A register for an address, which isn't r0, which reads as 0 there.
static u32 AddressReg(void)
{
  return 2 + R() % 29;
}

// out[0] = addi reg,r31,BUFFER+offset, or li reg,value: glued to the next instruction.
static void SetUp(u32* out, u8* g, u32 reg, u32 base, u32 value)
{
  out[0] = D(14, reg, base, value);
  g[0] = 1;
}

// A load's target, which isn't the address register of an update form.
static u32 LoadTarget(u32 address)
{
  u32 t = Dest();
  while (t == address)
    t = Dest();
  return t;
}

// In MMU mode, an access to a page of the segment, mapped or not, maybe across pages.
static u32 GenerateSegmentAccess(u32* out, u8* g, u32 room)
{
  static const u32 integer[] = {32, 34, 40, 42, 36, 38, 44, 33, 35, 41, 43, 37, 39, 45};
  static const u32 floating[] = {48, 50, 52, 54, 49, 51, 53, 55};
  if (!(config.flags & FLAG_MMU) || room < 3)
    return 0;
  const u32 b = AddressReg();
  const u32 page = R() % SEGMENT_PAGES;
  u32 offset = (R() & 3) ? R() % (512 - 16) : 4096 - (1 + R() % 8);
  out[0] = D(15, b, 0, SEGMENT >> 16);  // lis
  g[0] = 1;
  switch (R() % 3)
  {
  case 0:
  {
    const u32 op = integer[R() % 14];
    const u32 update = op & 1;
    const u32 load = op < 36 || op == 40 || op == 41 || op == 42 || op == 43;
    SetUp(out + 1, g + 1, b, b, page * 4096 + offset);
    out[2] = D(op, load ? LoadTarget(b) : Reg(), b, update ? (R() % 8) * 2 : 0);
    break;
  }
  case 1:
    offset &= ~7u;
    SetUp(out + 1, g + 1, b, b, page * 4096 + offset);
    out[2] = D(floating[R() % 8], Reg(), b, 0);
    break;
  default:
  {
    offset &= ~7u;
    const u32 wi = ((R() & 1) << 3) | (R() % 8);
    SetUp(out + 1, g + 1, b, b, page * 4096 + offset);
    out[2] = (((R() & 1) ? 56u : 60u) << 26) | (Reg() << 21) | (b << 16) | (wi << 12);
    break;
  }
  }
  return 3;
}

// Generates a unit of up to 8 instructions, the last of which uses what the others set up, if
// there's room for it: returns how many, or 0.
static u32 GenerateMemory(u32* out, u8* g, u32 room)
{
  switch (R() % 13)
  {
  case 11:
  {
    // Traps, which may or may not raise a program exception, and system calls.
    switch (R() % 3)
    {
    case 0:
      out[0] = D(3, R() % 32, Reg(), Imm16());
      break;
    case 1:
      out[0] = X(31, R() % 32, Reg(), Reg(), 4, 0);
      break;
    default:
      out[0] = 0x44000002;  // sc
      break;
    }
    return 1;
  }
  case 12:
    return GenerateSegmentAccess(out, g, room);
  case 0:
  {
    // Indexed, and byte-reversed.
    static const u32 loads[] = {23, 87, 279, 343, 534, 790};
    static const u32 stores[] = {151, 215, 407, 662, 918};
    if (room < 2)
      return 0;
    const u32 x = AddressReg();
    SetUp(out, g, x, 0, BUFFER + R() % (1024 - 8));
    if (R() & 1)
      out[1] = X(31, Dest(), 31, x, loads[R() % 6], 0);
    else
      out[1] = X(31, Reg(), 31, x, stores[R() % 5], 0);
    return 2;
  }
  case 1:
  {
    // Update forms, with a displacement.
    static const u32 loads[] = {33, 35, 41, 43};
    static const u32 stores[] = {37, 39, 45};
    if (room < 2)
      return 0;
    const u32 b = AddressReg();
    SetUp(out, g, b, 31, BUFFER + 64 + R() % (1024 - 128));
    const u32 d = (u32)(s32)(R() % 121) - 64;
    if (R() & 1)
      out[1] = D(loads[R() % 4], LoadTarget(b), b, d);
    else
      out[1] = D(stores[R() % 3], Reg(), b, d);
    return 2;
  }
  case 2:
  {
    // Update forms, indexed.
    static const u32 loads[] = {55, 119, 311, 375};
    static const u32 stores[] = {183, 247, 439};
    if (room < 3)
      return 0;
    const u32 b = AddressReg();
    u32 x = AddressReg();
    while (x == b)
      x = AddressReg();
    SetUp(out, g, b, 31, BUFFER + 64 + R() % (1024 - 128));
    SetUp(out + 1, g + 1, x, 0, (u32)(s32)(R() % 121) - 64);
    if (R() & 1)
      out[2] = X(31, LoadTarget(b), b, x, loads[R() % 4], 0);
    else
      out[2] = X(31, Reg(), b, x, stores[R() % 3], 0);
    return 3;
  }
  case 3:
  {
    // Floating-point: indexed, update, and update indexed.
    static const u32 indexed[] = {535, 599, 663, 727, 983};
    static const u32 update[] = {49, 51, 53, 55};
    static const u32 update_indexed[] = {567, 631, 695, 759};
    if (room < 3)
      return 0;
    const u32 b = AddressReg();
    switch (R() % 3)
    {
    case 0:
      SetUp(out, g, b, 0, BUFFER + (R() % 127) * 8);
      out[1] = X(31, Reg(), 31, b, indexed[R() % 5], 0);
      return 2;
    case 1:
      SetUp(out, g, b, 31, BUFFER + 64 + (R() % 112) * 8);
      out[1] = D(update[R() % 4], Reg(), b, (R() % 16) * 8 - 64);
      return 2;
    default:
    {
      u32 x = AddressReg();
      while (x == b)
        x = AddressReg();
      SetUp(out, g, b, 31, BUFFER + 64 + (R() % 112) * 8);
      SetUp(out + 1, g + 1, x, 0, (R() % 16) * 8 - 64);
      out[2] = X(31, Reg(), b, x, update_indexed[R() % 4], 0);
      return 3;
    }
    }
  }
  case 4:
  {
    // Quantized: indexed, update, and update indexed, with any GQR.
    if (room < 3)
      return 0;
    const u32 b = AddressReg();
    const u32 wi = ((R() & 1) << 3) | (R() % 8);
    switch (R() % 3)
    {
    case 0:
      SetUp(out, g, b, 0, BUFFER + (R() % 126) * 8);
      out[1] = (4u << 26) | (Reg() << 21) | (31 << 16) | (b << 11) | (wi << 7) |
               (((R() & 1) ? 6 : 7) << 1);
      return 2;
    case 1:
      SetUp(out, g, b, 31, BUFFER + 64 + (R() % 110) * 8);
      out[1] = (((R() & 1) ? 57u : 61u) << 26) | (Reg() << 21) | (b << 16) | (wi << 12) |
               (((R() % 16) * 8 - 64) & 0xFFF);
      return 2;
    default:
    {
      u32 x = AddressReg();
      while (x == b)
        x = AddressReg();
      SetUp(out, g, b, 31, BUFFER + 64 + (R() % 110) * 8);
      SetUp(out + 1, g + 1, x, 0, (R() % 16) * 8 - 64);
      out[2] = (4u << 26) | (Reg() << 21) | (b << 16) | (x << 11) | (wi << 7) |
               (((R() & 1) ? 38 : 39) << 1);
      return 3;
    }
    }
  }
  case 5:
  {
    // stmw, to the end of the registers.
    const u32 s = 2 + R() % 30;
    const u32 size = (32 - s) * 4;
    out[0] = D(47, s, 31, BUFFER + (R() % ((1024 - size) / 4 + 1)) * 4);
    return 1;
  }
  case 6:
  {
    // Strings: lswi into registers that don't wrap to r31, stswi, and stswx of XER's count.
    if (room < 2)
      return 0;
    const u32 nb = 1 + R() % 32;
    switch (R() % 3)
    {
    case 0:
    {
      const u32 n = (nb + 3) / 4;
      const u32 t = 2 + R() % (30 - n);
      u32 a = AddressReg();
      while (a >= t && a < t + n)
        a = AddressReg();
      SetUp(out, g, a, 31, BUFFER + R() % (1024 - 32));
      out[1] = X(31, t, a, nb & 31, 597, 0);
      return 2;
    }
    case 1:
    {
      const u32 a = AddressReg();
      SetUp(out, g, a, 31, BUFFER + R() % (1024 - 32));
      out[1] = X(31, Reg(), a, nb & 31, 725, 0);
      return 2;
    }
    default:
    {
      const u32 b = AddressReg();
      SetUp(out, g, b, 31, BUFFER + R() % (1024 - 128));
      out[1] = X(31, Reg(), 0, b, 661, 0);
      return 2;
    }
    }
  }
  case 7:
  {
    // A reservation.
    if (room < 2)
      return 0;
    const u32 b = AddressReg();
    SetUp(out, g, b, 31, BUFFER + (R() % 255) * 4);
    out[1] = (R() & 1) ? X(31, Dest(), 0, b, 20, 0) : X(31, Reg(), 0, b, 150, 1);
    return 2;
  }
  case 8:
  {
    // Cache instructions.
    static const u32 xo[] = {1014, 1014, 54, 86, 278, 246, 982};
    if (room < 2)
      return 0;
    const u32 b = AddressReg();
    SetUp(out, g, b, 31, BUFFER + R() % 1024);
    out[1] = X(31, 0, 0, b, xo[R() % 7], 0);
    return 2;
  }
  case 9:
  {
    // FPSCR's rounding mode, NI and XE from an FPR.
    out[0] = (63u << 26) | (1 << 17) | (Reg() << 11) | (711 << 1);
    return 1;
  }
  default:
  {
    // A floating-point record form, after clearing FPSCR's status bits, which it copies to CR1.
    if (room < 8)
      return 0;
    for (u32 f = 0; f < 7; f++)
    {
      out[f] = (63u << 26) | (f << 23) | (134 << 1);
      g[f] = 1;
    }
    switch (R() % 5)
    {
    case 0:
    {
      static const u32 xo[] = {21, 20, 25, 18, 29, 28, 31, 30, 23, 26};
      const u32 x = xo[R() % 10];
      out[7] = A(63, Reg(), x == 26 ? 0 : Reg(), x == 25 ? 0 : Reg(),
                 x == 25 || x >= 28 || x == 23 ? Reg() : 0, x, 1);
      break;
    }
    case 1:
    {
      static const u32 xo[] = {21, 20, 25, 18, 29, 28, 31, 30, 24};
      const u32 x = xo[R() % 9];
      out[7] = A(59, Reg(), x == 24 ? 0 : Reg(), x == 25 ? 0 : Reg(),
                 x == 25 || x >= 28 ? Reg() : 0, x, 1);
      break;
    }
    case 2:
    {
      static const u32 xo[] = {40, 72, 136, 264, 12, 14, 15};
      out[7] = X(63, Reg(), 0, Reg(), xo[R() % 7], 1);
      break;
    }
    case 3:
    {
      static const u32 xo[] = {21, 20, 25, 18, 29, 28, 31, 30, 10, 11, 12, 13, 14, 15, 23, 24, 26};
      const u32 x = xo[R() % 17];
      const u32 c = x == 25 || x == 12 || x == 13 || (x >= 28 && x <= 31) || x == 10 ||
                            x == 11 || x == 14 || x == 15 || x == 23 ?
                        Reg() :
                        0;
      out[7] = A(4, Reg(), x == 24 || x == 26 ? 0 : Reg(),
                 x == 25 || x == 12 || x == 13 ? 0 : Reg(), c, x, 1);
      break;
    }
    default:
    {
      static const u32 xo[] = {40, 72, 136, 264, 528, 560, 592, 624};
      const u32 x = xo[R() % 8];
      out[7] = X(4, Reg(), x >= 528 ? Reg() : 0, Reg(), x, 1);
      break;
    }
    }
    return 8;
  }
  }
}

static u32 Generate(u32* out, u8* g, u32 i, u32 length)
{
  if ((config.flags & FLAG_MMU) && R() % 8 == 0)
  {
    const u32 n = GenerateSegmentAccess(out, g, length - i);
    if (n != 0)
      return n;
  }
  if (R() % 4 == 0)
  {
    const u32 n = GenerateMemory(out, g, length - i);
    if (n != 0)
      return n;
  }
  return GenerateBasic(out, i, length);
}

static void FlushCode(void* start, u32 size)
{
  for (u32 i = 0; i < size; i += 32)
    asm volatile("dcbst 0,%0\n\tsync\n\ticbi 0,%0" : : "r"((u8*)start + i) : "memory");
  asm volatile("sync\n\tisync" : : : "memory");
}

static void SetGQRs(void)
{
  static const u32 types[] = {0, 4, 5, 6, 7};
  u32 g[8];
  for (u32 i = 1; i < 8; i++)
  {
    g[i] = ((R() % 64) << 24) | (types[R() % 5] << 16) | ((R() % 64) << 8) | types[R() % 5];
  }
  asm volatile("mtspr 913,%0\n\tmtspr 914,%1\n\tmtspr 915,%2\n\tmtspr 916,%3\n\t"
               "mtspr 917,%4\n\tmtspr 918,%5\n\tmtspr 919,%6"
               :
               : "r"(g[1]), "r"(g[2]), "r"(g[3]), "r"(g[4]), "r"(g[5]), "r"(g[6]), "r"(g[7]));
}

static u32 hash;
static void Mix(u32 value)
{
  hash ^= value;
  hash *= 0x01000193;
  hash ^= hash >> 15;
}
static void Mix64(u64 value)
{
  Mix((u32)(value >> 32));
  Mix((u32)value);
}

static void InstallHandler(u32 vector, const u32* start, const u32* end)
{
  u32* const destination = (u32*)(0x80000000 + vector);
  for (const u32* p = start; p < end; p++)
    destination[p - start] = *p;
  FlushCode(destination, 0x100);
}

// Maps each page of the segment, or not, with random R and C bits, cached or not.
static void MapPages(void)
{
  static const u32 bits[] = {0x180, 0x180, 0x100, 0x80, 0, 0x1A0};
  for (u32 page = 0; page < SEGMENT_PAGES; page++)
  {
    u32* const pte = PTE(page);
    if (R() % 4)
    {
      pte[0] = 0x80000000 | (SEGMENT_VSID << 7) | ((SEGMENT >> 22) & 0x3F);
      pte[1] = ((u32)pages[page] - 0x80000000) | bits[R() % 6] | 2;
    }
    else
    {
      pte[0] = 0;
      pte[1] = 0;
    }
    asm volatile("sync\n\ttlbie %0" : : "r"(SEGMENT + page * 4096) : "memory");
  }
  asm volatile("sync\n\ttlbsync\n\tsync" : : : "memory");
  for (u32 page = 0; page < SEGMENT_PAGES; page++)
  {
    for (u32 i = 0; i < 512; i++)
      pages[page][i] = R();
    for (u32 i = 4096 - 64; i < 4096; i++)
      pages[page][i] = R();
  }
}

int main(void)
{
  InstallHandler(0x300, dsi_handler, dsi_handler_end);
  InstallHandler(0x600, alignment_handler, alignment_handler_end);
  InstallHandler(0x700, program_handler, program_handler_end);
  InstallHandler(0xC00, syscall_handler, syscall_handler_end);
  if (config.flags & FLAG_MMU)
  {
    for (u32 i = 0; i < 0x10000 / 4; i++)
      PAGE_TABLE[i] = 0;
    asm volatile("mtsdr1 %0\n\tmtsr 6,%1\n\tsync\n\tisync"
                 :
                 : "r"((u32)PAGE_TABLE & 0x3FFFFFFF), "r"(SEGMENT_VSID)
                 : "memory");
  }

  for (u32 p = config.start; p < config.start + config.count; p++)
  {
    rng = (p + 1) * 0x9E3779B9u;
    for (u32 i = 0; i < 4; i++)
      R();

    const u32 length = 4 + R() % (MAX_LENGTH - 4);
    for (u32 i = 0; i < length; i++)
      glue[i] = 0;
    for (u32 i = 0; i < length;)
      i += Generate(&generated[i], &glue[i], i, length);
    // Branches into a unit go to its start instead, so that they don't skip what it sets up.
    for (u32 i = 0; i < length; i++)
    {
      const u32 w = generated[i];
      const u32 op = w >> 26;
      if (op != 16 && op != 18)
        continue;
      const u32 mask = op == 16 ? 0xFFFC : 0x3FFFFFC;
      u32 target = i + (w & mask) / 4;
      while (target < length && glue[target - 1])
        target--;
      generated[i] = (w & ~mask) | (((target - i) * 4) & mask);
    }
    // Replaced with nop, but what's glued to an instruction that stays stays too.
    code[length] = 0x4E800020;  // blr
    u32 next_kept = 1;
    for (u32 i = length; i-- > 0;)
    {
      const u32 nop = p == config.reduced && (config.nop[i / 32] & (1u << (i % 32)));
      const u32 kept = !nop || (glue[i] && next_kept);
      code[i] = kept ? generated[i] : 0x60000000;
      next_kept = kept;
    }
    FlushCode(code, sizeof(code));

    for (u32 i = 0; i < 32; i++)
    {
      state.gpr[i] = Value();
      state.ps0[i] = DoubleValue();
      state.ps1[i] = SingleValue();
    }
    state.cr = R();
    state.xer = R() & 0xE000FF7F;
    state.ctr = (R() & 1) ? R() % 8 : R();
    state.fpscr = R() % 8;  // The rounding mode and NI
    for (u32 i = 0; i < sizeof(state.buffer); i++)
      state.buffer[i] = R();
    SetGQRs();
    if (config.flags & FLAG_MMU)
      MapPages();

    FuzzRun(&state, code);

    hash = 0x811C9DC5;
    for (u32 i = 0; i < 32; i++)
    {
      if (i != 1 && i != 31)
        Mix(state.out_gpr[i]);
      Mix64(state.out_ps0[i]);
      Mix64(state.out_ps1[i]);
    }
    Mix(state.out_cr);
    Mix(state.out_xer);
    Mix(state.out_ctr);
    // The rounding mode, NI and the exception enables, and FPRF with it.
    Mix((u32)state.out_fpscr & (config.flags & FLAG_FPRF ? 0x1F0FF : 0xFF));
    Mix(state.out_exceptions);
    Mix(state.out_dar);
    Mix(state.out_dsisr);
    if (config.flags & FLAG_MMU)
    {
      for (u32 page = 0; page < SEGMENT_PAGES; page++)
      {
        Mix(PTE(page)[1]);
        for (u32 i = 0; i < 512; i += 4)
          Mix(*(u32*)&pages[page][i]);
        for (u32 i = 4096 - 64; i < 4096; i += 4)
          Mix(*(u32*)&pages[page][i]);
      }
    }
    for (u32 i = 0; i < sizeof(state.buffer); i += 4)
      Mix(*(u32*)&state.buffer[i]);
    OSReport("TEST %u %08x\n", p, hash);

    if (config.dump)
    {
      for (u32 i = 0; i < length; i++)
        OSReport("TEST code %u %08x\n", i, code[i]);
      for (u32 i = 0; i < 32; i++)
      {
        OSReport("TEST r%u %08x\n", i, state.out_gpr[i]);
        OSReport("TEST f%u %08x%08x %08x%08x\n", i, (u32)(state.out_ps0[i] >> 32),
                 (u32)state.out_ps0[i], (u32)(state.out_ps1[i] >> 32), (u32)state.out_ps1[i]);
      }
      OSReport("TEST cr %08x xer %08x ctr %08x fpscr %08x\n", state.out_cr, state.out_xer,
               state.out_ctr, (u32)state.out_fpscr & (config.flags & FLAG_FPRF ? 0x1F0FF : 0xFF));
      OSReport("TEST exceptions %08x dar %08x dsisr %08x\n", state.out_exceptions,
               state.out_dar, state.out_dsisr);
      if (config.flags & FLAG_MMU)
      {
        for (u32 page = 0; page < SEGMENT_PAGES; page++)
        {
          OSReport("TEST pte%u %08x\n", page, PTE(page)[1]);
          for (u32 i = 0; i < 4096; i += i == 512 - 16 ? 4096 - 64 - 512 + 16 : 16)
          {
            OSReport("TEST p%u_%u %08x %08x %08x %08x\n", page, i, *(u32*)&pages[page][i],
                     *(u32*)&pages[page][i + 4], *(u32*)&pages[page][i + 8],
                     *(u32*)&pages[page][i + 12]);
          }
        }
      }
      for (u32 i = 0; i < sizeof(state.buffer); i += 16)
      {
        OSReport("TEST m%u %08x %08x %08x %08x\n", i, *(u32*)&state.buffer[i],
                 *(u32*)&state.buffer[i + 4], *(u32*)&state.buffer[i + 8],
                 *(u32*)&state.buffer[i + 12]);
      }
    }
  }
  OSReport("TEST done\n");
  return 0;
}
